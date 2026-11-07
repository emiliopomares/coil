# boxes (SPEC-0023): Milestone 1. The app logic is a library so tests can drive it through the fake
# and through SDL's dummy driver; only main.cpp chooses the SDL3 and software backends.
add_library(coil_example_boxes boxes.cpp)
add_library(coil::example_boxes ALIAS coil_example_boxes)
target_include_directories(coil_example_boxes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(coil_example_boxes
    PUBLIC coil::platform coil::layout coil::render coil::render_software)
coil_set_warnings(coil_example_boxes)

add_executable(boxes main.cpp)
target_link_libraries(boxes PRIVATE coil::example_boxes coil::platform_sdl3)
coil_set_warnings(boxes)

add_subdirectory(tests)
