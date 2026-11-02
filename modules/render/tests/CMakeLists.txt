# Contract suite for implementations of RenderBackend (header-only). Backend tests link this target
# and instantiate the suite for their implementation (SPEC-0020).
add_library(coil_render_contract INTERFACE)
add_library(coil::render_contract ALIAS coil_render_contract)
target_include_directories(coil_render_contract INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/contract)
target_link_libraries(coil_render_contract INTERFACE coil::render GTest::gtest)

coil_add_tests(render
    SOURCES color_test.cpp draw_list_test.cpp painter_test.cpp recording_render_backend_test.cpp
    DEPENDS coil::render_testing coil::render_contract)
