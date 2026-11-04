# ADR-0051: post-configuration provenance, before product targets are generated.
# FetchContent_MakeAvailable has already configured JUCE. This is not a gate on
# configuration-time execution; configure only trusted sources.
# ANABASIS_JUCE_PATH keeps its existing independent local-checkout branch.
set(_anabasis_juce_override FALSE)
if(DEFINED FETCHCONTENT_SOURCE_DIR_JUCE AND NOT "${FETCHCONTENT_SOURCE_DIR_JUCE}" STREQUAL "")
    set(_anabasis_juce_override TRUE)
endif()

find_package(Git QUIET)
set(_anabasis_juce_git_root FALSE)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_SOURCE_DIR}" rev-parse --show-toplevel
        RESULT_VARIABLE _anabasis_juce_root_result
        OUTPUT_VARIABLE _anabasis_juce_root OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _anabasis_juce_root_error TIMEOUT 10)
    if(_anabasis_juce_root_result EQUAL 0)
        file(REAL_PATH "${juce_SOURCE_DIR}" _anabasis_juce_source_real)
        file(REAL_PATH "${_anabasis_juce_root}" _anabasis_juce_root_real)
        if(WIN32)
            string(TOLOWER "${_anabasis_juce_source_real}" _anabasis_juce_source_real)
            string(TOLOWER "${_anabasis_juce_root_real}" _anabasis_juce_root_real)
        endif()
        # An archive extracted inside another repository must not inherit that
        # unrelated repository's HEAD as its supposed JUCE identity.
        if(_anabasis_juce_source_real STREQUAL _anabasis_juce_root_real)
            set(_anabasis_juce_git_root TRUE)
        endif()
    endif()
endif()

if(_anabasis_juce_git_root)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE _anabasis_juce_git_result
        OUTPUT_VARIABLE _anabasis_juce_head OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _anabasis_juce_git_error TIMEOUT 10)
    if(NOT _anabasis_juce_git_result EQUAL 0)
        message(FATAL_ERROR "Anabasis: cannot verify JUCE source HEAD: ${_anabasis_juce_git_error}")
    endif()
    string(LENGTH "${ANABASIS_JUCE_TAG}" _anabasis_juce_pin_length)
    string(TOLOWER "${ANABASIS_JUCE_TAG}" _anabasis_juce_expected)
    if(_anabasis_juce_pin_length EQUAL 40 AND ANABASIS_JUCE_TAG MATCHES "^[0-9a-fA-F]+$"
       AND NOT _anabasis_juce_head STREQUAL _anabasis_juce_expected)
        message(FATAL_ERROR "Anabasis: JUCE source HEAD ${_anabasis_juce_head} does not match immutable pin ${ANABASIS_JUCE_TAG}; reset the fetched checkout or use ANABASIS_JUCE_PATH for an intentional comparison.")
    endif()
    message(STATUS "Anabasis: verified JUCE source HEAD ${_anabasis_juce_head} at ${juce_SOURCE_DIR}")
elseif(_anabasis_juce_override AND NOT EXISTS "${juce_SOURCE_DIR}/.git")
    message(WARNING "Anabasis: unverified JUCE source override at ${juce_SOURCE_DIR}: no Git metadata rooted at this source directory. Trusted offline/development archives remain supported, but no immutable commit verification is claimed. Release/CI pin evidence requires a verifiable Git checkout.")
else()
    # A normal fetched checkout, or an override with broken Git metadata, may
    # not silently fall back to the archive policy.
    message(FATAL_ERROR "Anabasis: cannot verify JUCE source Git identity at ${juce_SOURCE_DIR}. A normal FetchContent build requires a verifiable JUCE Git checkout. Trusted source archives must be selected explicitly with FETCHCONTENT_SOURCE_DIR_JUCE or ANABASIS_JUCE_PATH.")
endif()

unset(_anabasis_juce_override)
unset(_anabasis_juce_git_root)
unset(_anabasis_juce_root_result)
unset(_anabasis_juce_root)
unset(_anabasis_juce_root_error)
unset(_anabasis_juce_source_real)
unset(_anabasis_juce_root_real)
unset(_anabasis_juce_git_result)
unset(_anabasis_juce_git_error)
unset(_anabasis_juce_head)
unset(_anabasis_juce_pin_length)
unset(_anabasis_juce_expected)
