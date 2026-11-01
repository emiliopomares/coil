# Contract suites for implementations of the platform ports (header-only). Adapter tests link this
# target and instantiate the suites for their implementation (SPEC-0003).
add_library(coil_platform_contract INTERFACE)
add_library(coil::platform_contract ALIAS coil_platform_contract)
target_include_directories(coil_platform_contract INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/contract)
target_link_libraries(coil_platform_contract INTERFACE coil::platform GTest::gtest)

coil_add_tests(platform
    SOURCES clock_test.cpp fake_window_system_test.cpp
    DEPENDS coil::platform_testing coil::platform_contract)
