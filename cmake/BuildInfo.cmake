# Writes BuildInfo.h with the commit being built and its time, at every build and only when it changed, so that a new
# commit recompiles the one file that reads it. Run by the target deepwater_buildinfo: cmake -DSOURCE=... -DOUT=... -P
execute_process(COMMAND git -C "${SOURCE}" log -1 --format=%H OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git -C "${SOURCE}" log -1 --format=%ct OUTPUT_VARIABLE when OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT when)
    set(when 0)
endif()
set(text "#pragma once\n#define DEEPWATER_COMMIT \"${commit}\"\n#define DEEPWATER_COMMIT_TIME ${when}LL\n")
if(EXISTS "${OUT}")
    file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL text)
    file(WRITE "${OUT}" "${text}")
endif()
