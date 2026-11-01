# coil_add_module(<name> SOURCES <src>... [DEPENDS <target>...])
# Creates library coil_<name> with alias coil::<name>, public include dir include/ and private src/.
function(coil_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
    set(target coil_${name})
    add_library(${target} ${ARG_SOURCES})
    add_library(coil::${name} ALIAS ${target})
    target_include_directories(${target}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_features(${target} PUBLIC cxx_std_23)
    target_link_libraries(${target} PUBLIC ${ARG_DEPENDS})
    coil_set_warnings(${target})
endfunction()

# coil_add_tests(<name> SOURCES <src>... [DEPENDS <target>...])
# Creates GoogleTest executable coil_<name>_tests linked to coil::<name>; tests are discovered by CTest.
function(coil_add_tests name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS" ${ARGN})
    set(target coil_${name}_tests)
    add_executable(${target} ${ARG_SOURCES})
    target_link_libraries(${target} PRIVATE coil::${name} ${ARG_DEPENDS} GTest::gtest_main)
    coil_set_warnings(${target})
    gtest_discover_tests(${target})
endfunction()
