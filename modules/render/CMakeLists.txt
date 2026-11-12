coil_add_module(render
    SOURCES src/draw_list.cpp src/paint.cpp src/painter.cpp src/render_backend.cpp src/text.cpp
    DEPENDS coil::foundation coil::tasks)

# Test-support library: test doubles for the render ports. Linked only by test executables.
add_library(coil_render_testing testing/src/recording_render_backend.cpp
                                testing/src/fake_text_engine.cpp)
add_library(coil::render_testing ALIAS coil_render_testing)
target_include_directories(coil_render_testing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/testing/include)
target_link_libraries(coil_render_testing PUBLIC coil::render)
coil_set_warnings(coil_render_testing)

add_subdirectory(tests)
