coil_add_module(render
    SOURCES src/draw_list.cpp src/paint.cpp src/painter.cpp
    DEPENDS coil::foundation)

add_subdirectory(tests)
