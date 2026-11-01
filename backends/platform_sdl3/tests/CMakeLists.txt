coil_add_tests(platform_sdl3
    SOURCES sdl_window_system_test.cpp
    DEPENDS coil::platform_contract SDL3::SDL3)

# AC-6: a translation unit including only the public header, built without SDL's include path.
add_library(coil_platform_sdl3_public_header_only OBJECT EXCLUDE_FROM_ALL public_header_only.cpp)
target_link_libraries(coil_platform_sdl3_public_header_only PRIVATE coil::platform_sdl3)
coil_set_warnings(coil_platform_sdl3_public_header_only)
add_test(NAME gate.platform_sdl3.AC6_PublicHeaderNeedsNoSdl
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target coil_platform_sdl3_public_header_only)
