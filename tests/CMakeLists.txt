# Gate tests (SPEC-0001): prove that the quality gates actually reject bad code.
set(coil_check_script ${PROJECT_SOURCE_DIR}/tools/check.sh)
set(coil_expect_failure ${CMAKE_CURRENT_SOURCE_DIR}/gates/expect_failure.cmake)
set(coil_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# AC-5: a warning must fail compilation.
add_executable(coil_fixture_unused_variable EXCLUDE_FROM_ALL fixtures/warning/unused_variable.cpp)
coil_set_warnings(coil_fixture_unused_variable)
add_test(NAME gate.warnings_are_errors
    COMMAND ${CMAKE_COMMAND} "-DEXPECT_REGEX=error.*unused-variable|C4189" -P ${coil_expect_failure} --
            ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coil_fixture_unused_variable)

# AC-6: the format gate rejects badly formatted code and accepts well formatted code.
add_test(NAME gate.format_rejects_bad
    COMMAND ${CMAKE_COMMAND} "-DEXPECT_REGEX=clang-format-violations" -P ${coil_expect_failure} --
            ${coil_check_script} format ${coil_fixtures}/format/bad.cpp)
add_test(NAME gate.format_accepts_good
    COMMAND ${coil_check_script} format ${coil_fixtures}/format/good.cpp)

# AC-7: the tidy gate rejects naming violations.
add_test(NAME gate.tidy_rejects_bad_naming
    COMMAND ${CMAKE_COMMAND} "-DEXPECT_REGEX=readability-identifier-naming" -P ${coil_expect_failure} --
            ${coil_check_script} tidy ${coil_fixtures}/tidy/bad_naming.cpp)
