find_package(SDL3 CONFIG REQUIRED)

coil_add_backend(platform_sdl3
    SOURCES src/sdl_window_system.cpp
    DEPENDS coil::platform
    PRIVATE_DEPENDS SDL3::SDL3)

add_subdirectory(tests)
