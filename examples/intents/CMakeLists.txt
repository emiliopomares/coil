# intents (SPEC-0029): prints the intents produced by real input. The app logic is a library so
# tests can drive it through the fake and through SDL's dummy driver; only main.cpp chooses SDL3.
add_library(coil_example_intents intents.cpp)
add_library(coil::example_intents ALIAS coil_example_intents)
target_include_directories(coil_example_intents PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(coil_example_intents PUBLIC coil::foundation coil::platform coil::core)
coil_set_warnings(coil_example_intents)

add_executable(intents main.cpp)
target_link_libraries(intents PRIVATE coil::example_intents coil::platform_sdl3)
coil_set_warnings(intents)

add_subdirectory(tests)
