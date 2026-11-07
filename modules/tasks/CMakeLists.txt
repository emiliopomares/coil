coil_add_module(tasks SOURCES src/async_result.cpp DEPENDS coil::foundation)

# Test-support library: fakes for the tasks types (header-only). Linked only by test executables.
add_library(coil_tasks_testing INTERFACE)
add_library(coil::tasks_testing ALIAS coil_tasks_testing)
target_include_directories(coil_tasks_testing INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/testing/include)
target_link_libraries(coil_tasks_testing INTERFACE coil::tasks)

add_subdirectory(tests)
