coil_add_module(platform SOURCES src/window_system.cpp DEPENDS coil::foundation)

# Test-support library: fakes for the platform ports. Linked only by test executables.
add_library(coil_platform_testing testing/src/fake_window_system.cpp)
add_library(coil::platform_testing ALIAS coil_platform_testing)
target_include_directories(coil_platform_testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/testing/include)
target_link_libraries(coil_platform_testing PUBLIC coil::platform)
coil_set_warnings(coil_platform_testing)

add_subdirectory(tests)
