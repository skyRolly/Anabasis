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

project(Anabasis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# C++20 modules are not used (ADR-0008 decision B5); C++23 library features are
# admitted only behind feature-test macros AND a thin first-party abstraction.

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Options (all ANABASIS_*, ADR-0008 §Options) ----------------------------
option(ANABASIS_BUILD_TESTS      "Build the AnabasisTests + AnabasisStateTests console apps" ON)
option(ANABASIS_BUILD_STANDALONE "Build the Standalone format" ON)
set(ANABASIS_JUCE_PATH "" CACHE PATH "Local JUCE checkout (skips FetchContent — offline escape hatch)")
set(ANABASIS_BUILD_NUMBER "0" CACHE STRING "CI build number (CI passes the run number)")

# --- JUCE: version 9.0.0 pinned to the tag's IMMUTABLE COMMIT SHA -----------
# A tag NAME can be re-pointed upstream; the SHA cannot (ADR-0008 decision A5,
# OQ-001). GIT_SHALLOW with a SHA is a documented CMake trap: if the fetch
# misbehaves, drop GIT_SHALLOW, NEVER the SHA (ADR-0008's caveat carries the
# sanctioned fix). Changing this pin is an Architecture Review Gate item
# (DEPENDENCY_POLICY.md).
set(ANABASIS_JUCE_VERSION "9.0.0" CACHE STRING "Human-readable JUCE version (README records this)")
set(ANABASIS_JUCE_TAG "f8f8864172464b9adf9eba6101e1f784838d1597" CACHE STRING "Immutable commit SHA of the JUCE tag")

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
        target_link_options(AnabasisHardening INTERFACE -Wl,-dead_strip)
    else()
        target_link_options(AnabasisHardening INTERFACE
            -Wl,--gc-sections -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
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

target_compile_definitions(Anabasis PUBLIC
    ANABASIS_VERSION_STRING="${PROJECT_VERSION}"
    ANABASIS_BUILD_NUMBER="${ANABASIS_BUILD_NUMBER}"
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
        juce::juce_recommended_lto_flags
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
        ANABASIS_BUILD_NUMBER="${ANABASIS_BUILD_NUMBER}"
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
