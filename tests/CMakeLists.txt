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

# Layering gate tests (SPEC-0004). Each fixture under fixtures/layering/<case>/ has its own rules file.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(coil_layering ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/check_layering.py)
set(coil_expect_output ${CMAKE_CURRENT_SOURCE_DIR}/gates/expect_output.cmake)
set(coil_layering_fixtures ${coil_fixtures}/layering)

# Runs the checker on fixture <case>.
function(coil_layering_args out case)
    set(${out} ${coil_layering} --root ${coil_layering_fixtures}/${case}
        --config ${coil_layering_fixtures}/${case}/layering.json PARENT_SCOPE)
endfunction()

# AC-1
add_test(NAME gate.layering.AC1_RepositoryIsClean COMMAND ${coil_check_script} layering)

# AC-2
coil_layering_args(args module_edge_disallowed)
add_test(NAME gate.layering.AC2_RejectsDisallowedModuleEdge
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=modules/foundation/src/a\\.cpp:3: error: [^\n]*\\[module-edge\\]"
            -DEXPECT_COUNT_1=1 -DEXPECT_ERROR_LINES=1
            -P ${coil_expect_output} -- ${args})
coil_layering_args(args module_edge_indirect)
add_test(NAME gate.layering.AC2_RejectsIndirectEdge
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=modules/c/src/c\\.cpp:2: error: [^\n]*\\[module-edge\\]"
            -DEXPECT_COUNT_1=1 -DEXPECT_ERROR_LINES=1
            -P ${coil_expect_output} -- ${args})
coil_layering_args(args module_edge_direct_ok)
add_test(NAME gate.layering.AC2_AcceptsDirectEdgeAndFoundation COMMAND ${args})

# AC-3
coil_layering_args(args external_headers)
add_test(NAME gate.layering.AC3_RejectsExternalHeadersInModules
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=SDL3/SDL\\.h[^\n]*\\[external-header\\]" -DEXPECT_COUNT_1=1
            "-DEXPECT_REGEX_2=windows\\.h[^\n]*\\[external-header\\]" -DEXPECT_COUNT_2=1
            -DEXPECT_ERROR_LINES=2
            -P ${coil_expect_output} -- ${args})

# AC-4
coil_layering_args(args test_only_in_production)
add_test(NAME gate.layering.AC4_RejectsTestOnlyIncludesInProduction
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=\\[test-framework\\]" -DEXPECT_COUNT_1=1
            "-DEXPECT_REGEX_2=\\[test-support\\]" -DEXPECT_COUNT_2=2
            -DEXPECT_ERROR_LINES=3
            -P ${coil_expect_output} -- ${args})
coil_layering_args(args test_only_in_tests)
add_test(NAME gate.layering.AC4_AcceptsTestOnlyIncludesInTests COMMAND ${args})

# AC-5
coil_layering_args(args quote_and_unresolved)
add_test(NAME gate.layering.AC5_RejectsEscapingAndUnresolvedIncludes
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=src/a\\.cpp:2: error: [^\n]*\\[quote-include\\]" -DEXPECT_COUNT_1=1
            "-DEXPECT_REGEX_2=src/a\\.cpp:4: error: [^\n]*\\[unresolved-coil-header\\]"
            -DEXPECT_COUNT_2=1 -DEXPECT_ERROR_LINES=2
            -P ${coil_expect_output} -- ${args})
coil_layering_args(args configured_header)
add_test(NAME gate.layering.AC5_AcceptsConfiguredHeaderTemplate COMMAND ${args})

# AC-6
coil_layering_args(args backend_and_unknown)
add_test(NAME gate.layering.AC6_EnforcesBackendEdgesAndKnownFolders
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=1
            "-DEXPECT_REGEX_1=backends/sdl/src/s\\.cpp:3: error: [^\n]*\\[backend-edge\\]"
            -DEXPECT_COUNT_1=1
            "-DEXPECT_REGEX_2=modules/ghost[^\n]*\\[unknown-module\\]" -DEXPECT_COUNT_2=1
            -DEXPECT_ERROR_LINES=2
            -P ${coil_expect_output} -- ${args})

# AC-7
coil_layering_args(args config_cycle)
add_test(NAME gate.layering.AC7_RejectsConfigCycle
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=2
            "-DEXPECT_REGEX_1=(a -> b -> a|b -> a -> b)[^\n]*\\[config-cycle\\]" -DEXPECT_COUNT_1=1
            -P ${coil_expect_output} -- ${args})
coil_layering_args(args config_undefined)
add_test(NAME gate.layering.AC7_RejectsUndefinedDependency
    COMMAND ${CMAKE_COMMAND} -DEXPECT_EXIT=2 "-DEXPECT_REGEX_1=undefinedmodule" -DEXPECT_COUNT_1=1
            -P ${coil_expect_output} -- ${args})

# AC-8
add_test(NAME gate.layering.AC8_CheckShPropagatesFailure
    COMMAND ${CMAKE_COMMAND} "-DEXPECT_REGEX=\\[module-edge\\]" -P ${coil_expect_failure} --
            ${coil_check_script} layering --root ${coil_layering_fixtures}/module_edge_disallowed
            --config ${coil_layering_fixtures}/module_edge_disallowed/layering.json)
