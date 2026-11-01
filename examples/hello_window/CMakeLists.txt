# hello_window (SPEC-0007): the first composition root. The app logic is a library so tests can drive
# it through the fake and through SDL's dummy driver; only main.cpp chooses the SDL3 backend.
add_library(coil_example_hello_window hello_window.cpp)
add_library(coil::example_hello_window ALIAS coil_example_hello_window)
target_include_directories(coil_example_hello_window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(coil_example_hello_window PUBLIC coil::platform)
coil_set_warnings(coil_example_hello_window)

add_executable(hello_window main.cpp)
target_link_libraries(hello_window PRIVATE coil::example_hello_window coil::platform_sdl3)
coil_set_warnings(hello_window)

add_subdirectory(tests)
