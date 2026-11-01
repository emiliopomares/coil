find_package(SDL3 CONFIG REQUIRED)

coil_add_tests(example_hello_window
    SOURCES hello_window_test.cpp
    DEPENDS coil::platform_testing coil::platform_sdl3 SDL3::SDL3
    TIMEOUT 30)

# AC-6: the example executable is part of the default build (fails if the file does not exist).
add_test(NAME gate.examples.AC6_HelloWindowBuilds
    COMMAND ${CMAKE_COMMAND} -E md5sum $<TARGET_FILE:hello_window>)
