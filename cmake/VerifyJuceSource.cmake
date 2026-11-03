# ADR-0051: record the source actually consumed and reject stale immutable pins.
# Local ANABASIS_JUCE_PATH comparisons bypass this FetchContent-only check.
# Custom named revisions remain supported and report their resolved commit.
find_package(Git REQUIRED)
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
unset(_anabasis_juce_git_result)
unset(_anabasis_juce_git_error)
unset(_anabasis_juce_head)
unset(_anabasis_juce_pin_length)
unset(_anabasis_juce_expected)
