cmake_minimum_required(VERSION 3.28)

if("$ENV{VCPKG_ROOT}" STREQUAL "")
    message(FATAL_ERROR
        "VCPKG_ROOT is not set. Install vcpkg and export VCPKG_ROOT=<path to vcpkg> "
        "(see docs/specs/0001-build-skeleton.md, section 10.0).")
endif()

project(Coil VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COIL_BUILD_EXAMPLES "Build the example programs in examples/" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CoilWarnings)
include(CoilSanitizers)
include(CoilModule)

enable_testing()
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_subdirectory(modules/foundation)
add_subdirectory(modules/platform)
add_subdirectory(modules/layout)
add_subdirectory(modules/tasks)
add_subdirectory(modules/render)
add_subdirectory(modules/core)
add_subdirectory(backends/platform_sdl3)
add_subdirectory(backends/render_software)
if(COIL_BUILD_EXAMPLES)
    add_subdirectory(examples/hello_window)
    add_subdirectory(examples/boxes)
    add_subdirectory(examples/intents)
endif()
add_subdirectory(tests)
