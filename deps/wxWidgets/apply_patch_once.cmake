# Applies PATCH to the source in the working directory unless the source already has it.
# The deps fork took the colour patch into its v3.3.2 tag on 2026-10-23; old and new checkouts both build.
# Usage: cmake -DGIT=<git> -DPATCH=<file.patch> -P apply_patch_once.cmake
execute_process(COMMAND "${GIT}" apply --reverse --check "${PATCH}" RESULT_VARIABLE _already OUTPUT_QUIET ERROR_QUIET)
if (_already EQUAL 0)
    message(STATUS "Already in the source, skipped: ${PATCH}")
    return()
endif ()
execute_process(COMMAND "${GIT}" apply --verbose "${PATCH}" RESULT_VARIABLE _applied)
if (NOT _applied EQUAL 0)
    message(FATAL_ERROR "Patch does not apply: ${PATCH}")
endif ()
