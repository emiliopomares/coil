# Usage: cmake -DEXPECT_REGEX=<regex> -P expect_failure.cmake -- <command> [args...]
# Passes only if <command> exits non-zero AND its combined output matches EXPECT_REGEX, so a command
# that fails for an unrelated reason does not count as the gate working (SPEC-0001, rule 5).
if(NOT DEFINED EXPECT_REGEX)
    message(FATAL_ERROR "expect_failure: EXPECT_REGEX is not set")
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
    message(FATAL_ERROR "expect_failure: no command given after --")
endif()

execute_process(COMMAND ${command}
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
set(output "${stdout}${stderr}")

if(result EQUAL 0)
    message(FATAL_ERROR "Expected the command to fail, but it succeeded:\n  ${command}\n${output}")
endif()
if(NOT output MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR
        "Command failed (exit ${result}) but output does not match '${EXPECT_REGEX}':\n${output}")
endif()
message(STATUS "Failed as expected (exit ${result}), diagnostic matched '${EXPECT_REGEX}'")
