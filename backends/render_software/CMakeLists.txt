coil_add_backend(render_software
    SOURCES src/software_render_backend.cpp
    DEPENDS coil::render)

add_subdirectory(tests)
