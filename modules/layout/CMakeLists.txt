coil_add_module(layout
    SOURCES src/box_constraints.cpp src/layout_element.cpp src/linear_layout.cpp src/modifier.cpp
    DEPENDS coil::foundation)

add_subdirectory(tests)
