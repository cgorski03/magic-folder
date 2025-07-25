# Tests configuration
cmake_minimum_required(VERSION 3.20)

# Find Google Test
find_package(GTest CONFIG REQUIRED)

# Define all test source files
set(TEST_SOURCES
    test_main.cpp
    test_utilities.cpp
    test_file_info_service.cpp
    test_file_delete_service.cpp
    test_file_processing_service.cpp
    test_search_service.cpp
)

# Create single test executable for all tests
add_executable(magic_folder_tests ${TEST_SOURCES})

# Link against required libraries
target_link_libraries(magic_folder_tests 
    PRIVATE
    magic_core
    magic_services
    GTest::gtest 
    GTest::gtest_main
    GTest::gmock
)

# Set include directories
target_include_directories(magic_folder_tests 
    PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}  # For test_utilities.hpp
)

# Add test to CTest
add_test(NAME MagicFolderAllTests COMMAND magic_folder_tests)

# Enable testing features
include(GoogleTest)
gtest_discover_tests(magic_folder_tests)

# Optional: Create convenience targets for running specific test suites
add_custom_target(test_file_info_service
    COMMAND magic_folder_tests --gtest_filter="FileInfoServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileInfoService tests only"
)

add_custom_target(test_file_delete_service
    COMMAND magic_folder_tests --gtest_filter="FileDeleteServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileDeleteService tests only"
)

add_custom_target(test_file_processing_service
    COMMAND magic_folder_tests --gtest_filter="FileProcessingServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileProcessingService tests only"
)

# Add convenience target for search service tests
add_custom_target(test_search_service
    COMMAND magic_folder_tests --gtest_filter="SearchServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running SearchService tests only"
)
