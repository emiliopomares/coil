# Usage: cmake -DEXPECT_EXIT=<code>
#              [-DEXPECT_REGEX_<i>=<regex> -DEXPECT_COUNT_<i>=<n>]...   (i = 1..9)
#              [-DEXPECT_ERROR_LINES=<n>]
#              -P expect_output.cmake -- <command> [args...]
# Passes only if <command> exits with EXPECT_EXIT, each EXPECT_REGEX_<i> matches the combined output
# exactly EXPECT_COUNT_<i> times, and (if given) exactly EXPECT_ERROR_LINES lines contain ": error: ".
if(NOT DEFINED EXPECT_EXIT)
    message(FATAL_ERROR "expect_output: EXPECT_EXIT is not set")
endif()

set(command)
set(collecting FALSE)
math(EXPR last_index "${CMAKE_ARGC} - 1")
foreach(index RANGE ${last_index})
    if(collecting)
        list(APPEND command "${CMAKE_ARGV${index}}")
    elseif("${CMAKE_ARGV${index}}" STREQUAL "--")
        set(collecting TRUE)
    endif()
endforeach()
if(NOT command)
    message(FATAL_ERROR "expect_output: no command given after --")
endif()

execute_process(COMMAND ${command}
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
set(output "${stdout}${stderr}")

set(failures "")
if(NOT "${result}" STREQUAL "${EXPECT_EXIT}")
    string(APPEND failures "  exit code ${result}, expected ${EXPECT_EXIT}\n")
endif()
foreach(i RANGE 1 9)
    if(DEFINED EXPECT_REGEX_${i})
        string(REGEX MATCHALL "${EXPECT_REGEX_${i}}" matches "${output}")
        list(LENGTH matches count)
        if(NOT count EQUAL EXPECT_COUNT_${i})
            string(APPEND failures
                "  '${EXPECT_REGEX_${i}}' matched ${count} times, expected ${EXPECT_COUNT_${i}}\n")
        endif()
    endif()
endforeach()
if(DEFINED EXPECT_ERROR_LINES)
    string(REGEX MATCHALL ": error: " error_lines "${output}")
    list(LENGTH error_lines error_count)
    if(NOT error_count EQUAL EXPECT_ERROR_LINES)
        string(APPEND failures "  ${error_count} error lines, expected ${EXPECT_ERROR_LINES}\n")
    endif()
endif()

if(failures)
    message(FATAL_ERROR "Unexpected result of:\n  ${command}\n${failures}Output:\n${output}")
endif()
message(STATUS "Output matched all expectations (exit ${result})")
