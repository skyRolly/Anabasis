# ============================================================================
#  Anabasis — build architecture per ADR-0008 (Accepted 2026-07-31).
#
#  Five declared targets (item 4 of the ADR's target list declares two console
#  apps, so the numbered list runs to 4 while the target count is 5), plus the
#  format targets JUCE generates:
#    1. AnabasisHardening   — INTERFACE, behaviour-neutral binary hygiene
#    2. AnabasisDSP         — INTERFACE, the format-agnostic core (ADR-0001)
#    3. Anabasis            — juce_add_plugin (VST3 / AU-on-APPLE / Standalone)
#    4. AnabasisTests       — DSP acceptance console app (core only, no wrapper)
#       AnabasisStateTests  — state/parameter-compatibility console app
#                             (compiles the REAL wrapper + GUI sources)
#
#  Identity is FROZEN (ADR-0008 §Identity): RollyTech / RTec / Anbs /
#  com.rollytech.anabasis / VST3 "Fx" "Dynamics" "Mastering" / PRODUCT_NAME
#  "Anabasis" (an identity field too — the VST3 UID derives from it).
# ============================================================================
cmake_minimum_required(VERSION 3.22)

project(Anabasis VERSION 0.2.11 LANGUAGES C CXX)

# THE BASELINE IS C++23 (ADR-0030, 0.2.0), superseding ADR-0008 decision B5's
# C++20 and closing OQ-006. What used to sit here was a cache seam
# (`ANABASIS_CXX_STANDARD`, 20 default, 23 legal) whose ONE caller was the weekly
# canary asking "does tomorrow's baseline still compile?". That question is now
# answered permanently and by the main matrix rather than weekly and off to one
# side: every job on all three platforms compiles at 23. The seam and the canary
# workflow went with it -- an option with no caller is dead state, and this
# repository removes those rather than leaving them to imply a configuration
# somebody supports.
#
# The version is a HARD set, not a cache variable, deliberately: an unconditional
# `set()` shadows a same-named cache entry, so `-DCMAKE_CXX_STANDARD=20` would
# fail SILENTLY rather than configure. Changing the baseline is a Build System
# change under the Architecture Review Gate; the way to change it is an ADR and
# this line, not a command-line flag.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Modules are still not used (ADR-0008 decision B5's other half, which ADR-0030
# does NOT supersede); C++23 LIBRARY features are admitted only behind
# feature-test macros and a thin first-party abstraction, because the three
# shipped standard libraries do not implement them on the same schedule.

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Options (all ANABASIS_*, ADR-0008 §Options) ----------------------------
option(ANABASIS_BUILD_TESTS      "Build the AnabasisTests + AnabasisStateTests console apps" ON)
option(ANABASIS_BUILD_BENCH      "Build the AnabasisBench performance target (DESIGN §9; OFF by default)" OFF)
option(ANABASIS_BUILD_PROBE      "Build AnabasisChannelProbe, which HOSTS the built plugin bundle (KI-009; OFF by default)" OFF)
option(ANABASIS_BUILD_STANDALONE "Build the Standalone format" ON)
# DIAGNOSTIC ONLY, and OFF everywhere except the KI-009 macOS x86_64 job. ON
# compiles src/dsp/StageTrace.h's accumulators into the engine, so every stage
# of the chain reports per-channel RMS at `releaseResources`. It changes the
# engine's timing and is not realtime-safe; a binary built with it must never be
# shipped or measured for performance. OFF expands every tap to `((void) 0)`.
option(ANABASIS_STAGE_TRACE      "Compile the KI-009 per-stage per-channel trace into the engine (DIAGNOSTIC; never ship)" OFF)
# BISECTION ONLY. ON drops `juce_recommended_lto_flags` from every target that
# carries it, which changes what the optimiser is allowed to do ACROSS
# translation units. It exists because KI-009 turned out to be visible only in
# an LTO build under one compiler, and "does it survive without LTO" is the
# first question any such report has to answer. The shipping configuration is
# OFF; a binary built with this ON is not the product.
option(ANABASIS_NO_LTO           "Build WITHOUT juce_recommended_lto_flags (bisection; the shipped binary always has them)" OFF)
if(ANABASIS_NO_LTO)
    set(ANABASIS_LTO_FLAGS "")
    message(STATUS "Anabasis: LTO DISABLED -- bisection build, not the shipping configuration")
else()
    set(ANABASIS_LTO_FLAGS juce::juce_recommended_lto_flags)
endif()
set(ANABASIS_JUCE_PATH "" CACHE PATH "Local JUCE checkout (skips FetchContent — offline escape hatch)")
set(ANABASIS_BUILD_NUMBER "0" CACHE STRING "CI build number (CI passes the run number)")

# --- JUCE: version 9.0.1 pinned to the tag's IMMUTABLE COMMIT SHA -----------
# A tag NAME can be re-pointed upstream; the SHA cannot (ADR-0008 decision A5,
# OQ-001). GIT_SHALLOW with a SHA is a documented CMake trap: if the fetch
# misbehaves, drop GIT_SHALLOW, NEVER the SHA (ADR-0008's caveat carries the
# sanctioned fix). Changing this pin is an Architecture Review Gate item
# (DEPENDENCY_POLICY.md); 9.0.0 -> 9.0.1 is ADR-0028.
set(ANABASIS_JUCE_VERSION "9.0.1" CACHE STRING "Human-readable JUCE version (README records this)")
set(ANABASIS_JUCE_TAG "e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8" CACHE STRING "Immutable commit SHA of the JUCE tag")

if(ANABASIS_JUCE_PATH)
    add_subdirectory(${ANABASIS_JUCE_PATH} ${CMAKE_BINARY_DIR}/juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${ANABASIS_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ============================================================================
#  1. AnabasisHardening — behaviour-neutral binary hygiene as usage
#     requirements, linked PUBLIC so the flags reach every format target's
#     compile AND link, and the test targets too (which is what makes the
#     self-tests validate the shipped flag configuration).
#     Deliberately absent, and the absence is part of ADR-0008: -O3,
#     -ffast-math and extra LTO — numerics-affecting flags are frozen by DSP
#     policy, and LTO already arrives through juce_recommended_lto_flags.
#     Symbol stripping happens in CI packaging, never here.
# ============================================================================
add_library(AnabasisHardening INTERFACE)
if(MSVC)
    target_compile_options(AnabasisHardening INTERFACE /guard:cf $<$<CONFIG:Release>:/Zi>)
    target_link_options(AnabasisHardening INTERFACE
        /guard:cf /DYNAMICBASE /NXCOMPAT
        $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
else()
    target_compile_options(AnabasisHardening INTERFACE
        -fstack-protector-strong -ffunction-sections -fdata-sections
        $<$<CONFIG:Release>:-g>)
    if(APPLE)
        # ------------------------------------------------------------------
        #  -object_path_lto: the one flag that makes macOS crash symbolication
        #  EXIST (0.2.0). Not an optimisation and not a hardening flag -- a fix
        #  for a contract this project claimed to meet and did not.
        #
        #  WHAT WAS BROKEN. `dsymutil` does not read DWARF out of the linked
        #  binary; it reads the binary's DEBUG MAP (the `N_OSO` stabs), walks
        #  back to each object file the linker consumed, and pulls the DWARF
        #  from there. Under `-flto` -- which `juce_recommended_lto_flags` puts
        #  on the shipped plugin, and ADR-0008 puts it there alone -- ld64 does
        #  codegen itself, writes the merged result to a TEMPORARY object, and
        #  deletes it when the link finishes. The OSO entry then names a path
        #  that no longer exists, `dsymutil` warns "unable to open object file",
        #  exits 0, and emits a dSYM with no usable DWARF -- which the packaging
        #  step correctly discards. The net effect was that `Anabasis-macOS-debug`
        #  was never produced, on the one platform whose users routinely hand a
        #  developer an OS crash log. Linux and Windows were unaffected:
        #  `objcopy --only-keep-debug` and MSVC's `/DEBUG` both read the LINKED
        #  image.
        #
        #  The macOS packaging step named this exact change as "a deliberate
        #  future decision, not a silent addition". This is that decision,
        #  taken: it is recorded in the 0.2.0 migration round rather than
        #  slipped in beside the packaging code.
        #
        #  WHAT IT CHANGES, precisely: WHERE ld64 writes that temporary. The
        #  bitcode, the codegen and the shipped bytes are untouched -- LTO is
        #  still LTO, and this is emphatically NOT a workaround that turns it
        #  off. The macOS jobs assert both halves after the build: that the
        #  directory below is non-empty (so LTO really ran) and that a
        #  UUID-matched dSYM with compile units came out (so the fix worked).
        #
        #  ONE PATH PER (TARGET, ARCHITECTURE) -- and the previous revision of
        #  this comment had the reason exactly backwards, which is why arm64
        #  shipped without symbols for four releases (ADR-0035). It read:
        #
        #     "IT MUST BE A DIRECTORY, not a file. [...] Given a directory,
        #      ld64 generates a unique name per link invocation, so the two
        #      slices -- and the three format targets that link separately --
        #      coexist."
        #
        #  ld64 does no such thing. From its own source, `ld/parsers/
        #  lto_file.cpp` in the full-LTO path:
        #
        #     if ( stat(object_path.c_str(), &statBuffer) == 0
        #          && S_ISDIR(statBuffer.st_mode) )
        #         object_path += "/lto.o";
        #
        #  A directory gets the FIXED name `lto.o` appended -- no architecture
        #  in it, no target in it. So every link wrote the same file. A
        #  universal build links ONCE PER ARCHITECTURE (the driver runs `ld`
        #  per `-arch` then `lipo`s), and several targets here link with LTO,
        #  so one build performed six links into one path and five of them were
        #  overwritten. The survivor was one target's x86_64 object: `dsymutil`
        #  then reported `(arm64) [...] No object file for requested
        #  architecture` for every bundle, and `could not find symbol` for the
        #  x86_64 slices of the two bundles that were not the survivor.
        #
        #  A FILE path is what actually gives uniqueness, because the same
        #  branch above uses a non-directory path verbatim. `$<TARGET_PROPERTY:
        #  NAME>` separates the targets and `-Xarch_<arch>` separates the
        #  slices, so each link owns its own object and none can clobber
        #  another.
        #
        #  THREE THINGS THIS FORM DEPENDS ON, each load-bearing:
        #    * `-Xarch_<arch>` forwards exactly ONE following token, and clang
        #      errors on more (`err_drv_invalid_Xarch_argument_with_args`).
        #      That is why the linker argument is the single comma-joined
        #      `-Wl,-object_path_lto,<path>` rather than `-Xlinker` pairs.
        #    * `SHELL:` is REQUIRED. `target_link_options` de-duplicates and
        #      recombines otherwise, which would split the `-Xarch_` from the
        #      argument it governs and silently re-create the collision.
        #    * Full LTO, not ThinLTO. ThinLTO's branch in the same file MKDIRs
        #      its argument and names members `<n>.o` -- still target- and
        #      arch-agnostic. Switching to `-flto=thin` reintroduces this bug;
        #      the CI assertion on architecture coverage is what would catch it.
        #
        #  The parent directory is created at CONFIGURE time; the object files
        #  inside it are named, not created, so ld64 writes each one itself.
        set(ANABASIS_LTO_OBJECT_DIR "${CMAKE_BINARY_DIR}/lto-objects")
        file(MAKE_DIRECTORY "${ANABASIS_LTO_OBJECT_DIR}")
        set(ANABASIS_LTO_LINK_OPTS "")
        if(CMAKE_OSX_ARCHITECTURES)
            foreach(_anabasis_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
                list(APPEND ANABASIS_LTO_LINK_OPTS
                    "SHELL:-Xarch_${_anabasis_arch} -Wl,-object_path_lto,${ANABASIS_LTO_OBJECT_DIR}/$<TARGET_PROPERTY:NAME>-${_anabasis_arch}.o")
            endforeach()
        else()
            # No explicit architecture list: one link, so the target name alone
            # is unique and `-Xarch_` would name an architecture that may not
            # be built.
            list(APPEND ANABASIS_LTO_LINK_OPTS
                "-Wl,-object_path_lto,${ANABASIS_LTO_OBJECT_DIR}/$<TARGET_PROPERTY:NAME>.o")
        endif()
        target_link_options(AnabasisHardening INTERFACE
            -Wl,-dead_strip
            ${ANABASIS_LTO_LINK_OPTS})
    else()
        target_link_options(AnabasisHardening INTERFACE
            -Wl,--gc-sections -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)

        # CLANG ON LINUX LINKS WITH LLD, and this is a correctness requirement
        # rather than a preference.
        #
        # GNU ld scans a static archive ONCE, in command-line order, selecting
        # the members that resolve the symbols undefined at that moment. Under
        # `-flto` the archive members are BITCODE: the plugin runs codegen only
        # after that scan, and the merged object it emits needs symbols the scan
        # already passed over — out-of-line copies, vtables and typeinfo that
        # only became referenced once the optimiser had inlined and devirtualised
        # across translation units. ld.bfd does not rescan, so those become
        # undefined references. The failure is spectacular and misleading: the
        # plugin link reports hundreds of missing JUCE symbols that are demonstrably
        # present in `libAnabasis_SharedCode.a`.
        #
        # Whether it fires at all depends on the binutils/plugin pairing — the
        # same source, same compiler and same flags link cleanly against one
        # `/usr/lib/bfd-plugins` and fail against another. That is precisely why
        # this is pinned here rather than worked around in one CI job: a build
        # that happens to succeed on the maintainer's machine and fails on a
        # runner is not a build configuration, it is a coincidence.
        #
        # lld ships with the compiler that produced the bitcode, so it is always
        # version-matched, and it resolves LTO symbol requests against archives
        # correctly. Probed rather than assumed: where it is absent the build
        # falls back to the default linker with a warning that names the hazard,
        # because refusing to configure would be a worse failure than a link that
        # may well succeed.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            include(CheckLinkerFlag)
            check_linker_flag(CXX "-fuse-ld=lld" ANABASIS_HAVE_LLD)
            if(ANABASIS_HAVE_LLD)
                target_link_options(AnabasisHardening INTERFACE -fuse-ld=lld)
            else()
                message(WARNING
                    "Anabasis: Clang on Linux without lld. LTO links through GNU ld can fail with "
                    "undefined references to symbols that ARE in libAnabasis_SharedCode.a (one-pass "
                    "archive scan vs post-scan LTO codegen). Install lld, or configure with "
                    "-DANABASIS_NO_LTO=ON if a link fails that way.")
            endif()
        endif()
    endif()
endif()

# ============================================================================
#  2. AnabasisDSP — INTERFACE (not STATIC — ADR-0001): sources compile into
#     each consuming final target, so the core is proven format-agnostic at
#     build level (DSP_POLICY invariant 13) and no JUCE module object code is
#     duplicated. Depends on juce_dsp / juce_audio_basics and nothing else.
# ============================================================================
add_library(AnabasisDSP INTERFACE)
target_include_directories(AnabasisDSP INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp)
target_sources(AnabasisDSP INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/AnabasisEngine.cpp)
target_link_libraries(AnabasisDSP INTERFACE juce::juce_dsp juce::juce_audio_basics)

# Carried on AnabasisDSP rather than on the plugin so that the trace reaches
# EVERY consumer of the engine at once — plugin, both test apps, the bench. A
# diagnostic that only the plugin could see would force a bundle rebuild to
# answer a question the state tests can already ask.
if(ANABASIS_STAGE_TRACE)
    target_compile_definitions(AnabasisDSP INTERFACE ANABASIS_STAGE_TRACE=1)
    message(STATUS "Anabasis: STAGE TRACE ENABLED -- diagnostic build, do not ship or benchmark this binary")
endif()

# ============================================================================
#  3. The plugin target.
# ============================================================================
set(ANABASIS_FORMATS VST3)
if(APPLE)
    list(APPEND ANABASIS_FORMATS AU)   # Logic Pro loads only AU
endif()
if(ANABASIS_BUILD_STANDALONE)
    list(APPEND ANABASIS_FORMATS Standalone)
endif()

juce_add_plugin(Anabasis
    COMPANY_NAME                "RollyTech"
    BUNDLE_ID                   "com.rollytech.anabasis"
    PLUGIN_MANUFACTURER_CODE    RTec        # vendor-wide — host IDENTITY, frozen (ADR-0008)
    PLUGIN_CODE                 Anbs        # frozen (ADR-0008)
    FORMATS                     ${ANABASIS_FORMATS}
    PRODUCT_NAME                "Anabasis"  # identity field too: the VST3 UID derives from it
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Dynamics" "Mastering"
    DESCRIPTION                 "Mastering loudness maximizer")

# One source list, two consumers (BINDING — ADR-0008): the plugin target and
# AnabasisStateTests both read this variable verbatim, so a wrapper/GUI file
# can never be added to only one of them.
set(ANABASIS_PLUGIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PluginParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MacroEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/LoudnessMeterView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/GrHistoryView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/SpectrumView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/CurveView.cpp)

target_sources(Anabasis PRIVATE ${ANABASIS_PLUGIN_SOURCES})
target_include_directories(Anabasis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# THE BUILD NUMBER IS ATTACHED TO THE ONE TRANSLATION UNIT THAT READS IT, not
# to the targets (0.2.6, family parity -- the sibling carries the identical
# arrangement with its own measurement). It is the only definition in this
# file whose VALUE changes on every CI run -- build.yml passes
# ${{ github.run_number }} -- and as a target-wide PUBLIC definition it sat on
# the command line of every translation unit in the plugin and the state
# suite, JUCE included. ccache's direct mode hashes the command line, so all
# of those TUs were guaranteed direct-mode misses on every run: the 0.2.4
# ccache round measured exactly that shape on the macOS universal lane -- 58
# direct vs 116 preprocessed hits, the direct hits concentrated in
# AnabasisTests, the one target that never carried the define. Scoped to the
# single reader, everything else direct-hits and two objects recompile.
#
# src/gui/PluginEditor.cpp already carries `#ifndef ANABASIS_BUILD_NUMBER ->
# "0"` (:116-117), so this is the same value through a narrower door, not a
# new contract. Source-file properties are directory-scoped; both consumers --
# the plugin target and AnabasisStateTests -- compile the file from this
# directory, so both still see the real number. Nothing else reads it:
# `grep -rn ANABASIS_BUILD_NUMBER src/` matches PluginEditor.cpp alone.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/gui/PluginEditor.cpp
    PROPERTIES COMPILE_DEFINITIONS ANABASIS_BUILD_NUMBER="${ANABASIS_BUILD_NUMBER}")

target_compile_definitions(Anabasis PUBLIC
    ANABASIS_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# juce_opengl is linked UNCONDITIONALLY; only the ATTACH is platform-gated at
# runtime (DESIGN §6.1: attach on macOS/Windows, never Linux/X11) — ADR-0008.
target_link_libraries(Anabasis
    PRIVATE
        AnabasisDSP
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        AnabasisHardening
        juce::juce_recommended_config_flags
        ${ANABASIS_LTO_FLAGS}
        juce::juce_recommended_warning_flags)

# ============================================================================
#  4. Test targets (single option gate; neither links the LTO-flags target).
# ============================================================================
if(ANABASIS_BUILD_TESTS)
    # -- AnabasisTests: DSP acceptance. Compiles the AnabasisDSP sources
    #    directly, no wrapper — this is what makes DSP_POLICY invariant 13's
    #    build-level format-agnosticism test writable as stated.
    juce_add_console_app(AnabasisTests PRODUCT_NAME "AnabasisTests")
    target_sources(AnabasisTests PRIVATE tests/dsp_tests.cpp)
    target_compile_definitions(AnabasisTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnabasisTests
        PRIVATE
            AnabasisDSP
        PUBLIC
            AnabasisHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)


    # -- AnabasisStateTests: state / parameter-compatibility. Compiles
    #    tests/state_tests.cpp plus the shared wrapper/GUI source list to
    #    exercise the REAL AnabasisAudioProcessor. Links the same PRIVATE set
    #    as the plugin target — juce_opengl included, because it compiles the
    #    GUI sources and therefore the editor's context member (ADR-0008).
    juce_add_console_app(AnabasisStateTests PRODUCT_NAME "AnabasisStateTests")
    target_sources(AnabasisStateTests PRIVATE
        tests/state_tests.cpp
        ${ANABASIS_PLUGIN_SOURCES})
    target_include_directories(AnabasisStateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnabasisStateTests PUBLIC
        ANABASIS_VERSION_STRING="${PROJECT_VERSION}"
        # ANABASIS_BUILD_NUMBER is attached to src/gui/PluginEditor.cpp as a
        # source-file property instead -- see the set_source_files_properties()
        # call and its reasoning above the plugin target. This target compiles
        # ${ANABASIS_PLUGIN_SOURCES}, so it still receives it there.
        ANABASIS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnabasisStateTests
        PRIVATE
            AnabasisDSP
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_opengl
        PUBLIC
            AnabasisHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# -- AnabasisBench: the DESIGN §9 measurement procedure. OFF by default —
#    a benchmark that always builds gets run casually and quoted without
#    its machine, which is the C2 failure the budget doc guards against.
#    OUTSIDE the ANABASIS_BUILD_TESTS block on purpose: the option is
#    documented as independent, and nested it made
#    `-DANABASIS_BUILD_BENCH=ON -DANABASIS_BUILD_TESTS=OFF` build nothing
#    at all — silently, which is the worst way for an option to be a lie.
#    It needs only AnabasisDSP, which is defined above either way.
if(ANABASIS_BUILD_BENCH)
    juce_add_console_app(AnabasisBench PRODUCT_NAME "AnabasisBench")
    target_sources(AnabasisBench PRIVATE tests/bench.cpp)
    target_compile_definitions(AnabasisBench PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnabasisBench
        PRIVATE
            AnabasisDSP
        PUBLIC
            AnabasisHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# -- AnabasisChannelProbe: the KI-009 runtime probe. It is the only target that
#    LOADS the built bundle instead of compiling the sources, which is the whole
#    point of it: `AnabasisStateTests` drives the real `processBlock`, but as a
#    console app built from the same sources — no LTO (the flags target is on the
#    plugin alone, above), no format wrapper, and on macOS one slice rather than
#    the universal artefact a user installs. The configuration the field report
#    describes had therefore never been asserted on.
#
#    OFF by default and outside ANABASIS_BUILD_TESTS, for the reasons the bench
#    block states: it needs a BUILT BUNDLE to point at, so it is meaningless in a
#    tree that has not produced one, and a target that silently builds nothing
#    is worse than an absent one.
#
#    It links the plugin HOSTING modules, which nothing else here does — hence
#    JUCE_PLUGINHOST_VST3/AU rather than the plugin-client defines used above.
if(ANABASIS_BUILD_PROBE)
    juce_add_console_app(AnabasisChannelProbe PRODUCT_NAME "AnabasisChannelProbe")
    target_sources(AnabasisChannelProbe PRIVATE tools/channel_probe.cpp)
    target_compile_definitions(AnabasisChannelProbe PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=$<IF:$<BOOL:${APPLE}>,1,0>
        # The probe PUMPS the message loop between audio blocks so MacroEngine's
        # 30 ms mapping timer actually fires — the macro path is otherwise dead
        # in any headless process, which is how the channel battery stayed
        # vacuous for six rounds. `runDispatchLoopUntil` is gated on this.
        # Test-tool only; the plugin target does not define it.
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnabasisChannelProbe
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            AnabasisHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    # The same reproduction WITHOUT the wrapper: engine in, engine out. It links
    # `juce_recommended_lto_flags`, which no other console target does, because
    # the plugin does — a defect the optimiser only commits across translation
    # units at link time is invisible to every target that omits them, which is
    # how KI-009 stayed a "plugin-only" fault for nine rounds.
    juce_add_console_app(AnabasisEngineRepro PRODUCT_NAME "AnabasisEngineRepro")
    target_sources(AnabasisEngineRepro PRIVATE tools/engine_repro.cpp)
    target_compile_definitions(AnabasisEngineRepro PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnabasisEngineRepro
        PRIVATE
            AnabasisDSP
        PUBLIC
            AnabasisHardening
            juce::juce_recommended_config_flags
            ${ANABASIS_LTO_FLAGS}
            juce::juce_recommended_warning_flags)
endif()
