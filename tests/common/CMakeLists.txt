# Common test utilities library
# This library contains shared utilities, mocks, and helpers used across all tests

# Define common test utility sources
set(COMMON_TEST_SOURCES
    utilities_test.cpp
)

# Define common test utility headers
set(COMMON_TEST_HEADERS
    utilities_test.hpp
    mocks_test.hpp
)

# Create a static library for common test utilities
add_library(magic_test_common STATIC ${COMMON_TEST_SOURCES})

# Link dependencies needed by test utilities
target_link_libraries(magic_test_common
    PUBLIC
        magic_core
        GTest::gtest
        GTest::gmock
        utf8cpp::utf8cpp
)

# Set include directories
target_include_directories(magic_test_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}    # For utilities_test.hpp, mocks_test.hpp
        ${CMAKE_SOURCE_DIR}/include    # For magic_core headers
)

# Set C++ standard
target_compile_features(magic_test_common PUBLIC cxx_std_20)

# Install headers for use by other test modules
set_target_properties(magic_test_common PROPERTIES
    PUBLIC_HEADER "${COMMON_TEST_HEADERS}"
) 