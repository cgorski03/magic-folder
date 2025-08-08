# Service layer unit tests
# Tests for all service classes in magic_core/services/

# Define service test sources
set(SERVICES_TEST_SOURCES
    compression_service_test.cpp
    file_processing_service_test.cpp
    search_service_test.cpp
)

# Create services test library
add_library(magic_test_services STATIC ${SERVICES_TEST_SOURCES})

# Link against common test utilities and required libraries
target_link_libraries(magic_test_services
    PUBLIC
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gmock
)

# Set include directories
target_include_directories(magic_test_services
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common
)

# Set C++ standard
target_compile_features(magic_test_services PUBLIC cxx_std_20)

# Define individual test targets for services
add_custom_target(test_services
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all service tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_compression_service
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="CompressionServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running CompressionService tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_file_processing_service
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="FileProcessingServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileProcessingService tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_search_service
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="SearchServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running SearchService tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 