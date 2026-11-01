coil_add_tests(foundation SOURCES error_test.cpp version_test.cpp)

# The expected version comes straight from the CMake project, so the test does not re-read the
# header under test (SPEC-0001, section 10.3).
target_compile_definitions(coil_foundation_tests PRIVATE
    COIL_TEST_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    COIL_TEST_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    COIL_TEST_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    COIL_TEST_EXPECTED_VERSION_STRING="${PROJECT_VERSION}")
