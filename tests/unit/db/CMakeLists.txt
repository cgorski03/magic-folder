# Database layer unit tests
# Tests for database and data access layer classes

# Define database test sources
set(DB_TEST_SOURCES
    metadata_store_test.cpp
    file_info_service_test.cpp
    file_delete_service_test.cpp
)

# Create database test library
add_library(magic_test_db STATIC ${DB_TEST_SOURCES})

# Link against common test utilities and required libraries
target_link_libraries(magic_test_db
    PUBLIC
        magic_test_common
        magic_core
        GTest::gtest
        GTest::gmock
)

# Set include directories
target_include_directories(magic_test_db
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/common
)

# Set C++ standard
target_compile_features(magic_test_db PUBLIC cxx_std_20)

# Define individual test targets for database layer
add_custom_target(test_db
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*StoreTest*:*InfoServiceTest*:*DeleteServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all database layer tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_metadata_store
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="MetadataStoreTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running MetadataStore tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_file_info_service
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="FileInfoServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileInfoService tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_file_delete_service
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="FileDeleteServiceTest.*"
    DEPENDS magic_folder_tests
    COMMENT "Running FileDeleteService tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 