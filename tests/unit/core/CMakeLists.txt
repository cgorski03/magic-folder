# Core functionality unit tests
# Tests for core types, utilities, and fundamental classes

set(CORE_TEST_SOURCES
    worker_test.cpp
    worker_pool_test.cpp
)

# Create core tests library
add_library(magic_test_core STATIC ${CORE_TEST_SOURCES})

# Link against common test utilities and required libraries
target_link_libraries(magic_test_core
    PUBLIC
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gmock
)

# Set include directories
target_include_directories(magic_test_core
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common
)

# Set C++ standard
target_compile_features(magic_test_core PUBLIC cxx_std_20)

# Define test targets for core functionality
add_custom_target(test_core
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*WorkerTest*:*WorkerPoolTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all core tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 