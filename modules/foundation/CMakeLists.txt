coil_add_module(foundation SOURCES src/error.cpp src/version.cpp)

# The project version is the single source of truth for coil::foundation::kVersion.
set(coil_foundation_generated ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
configure_file(include/coil/foundation/version_config.hpp.in
    ${coil_foundation_generated}/coil/foundation/version_config.hpp @ONLY)
target_include_directories(coil_foundation PUBLIC ${coil_foundation_generated})

add_subdirectory(tests)
