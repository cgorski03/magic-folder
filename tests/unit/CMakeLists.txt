# Unit tests coordination
# This file includes all unit test modules and creates coordinated targets

# Include all unit test modules
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(extractors)
add_subdirectory(db)

# Create a comprehensive unit test target
add_custom_target(test_unit
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="-*Integration*"
    DEPENDS magic_folder_tests
    COMMENT "Running all unit tests (excluding integration tests)"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Create layer-specific test targets
add_custom_target(test_unit_services
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all service layer unit tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_unit_extractors
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*ExtractorTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all extractor unit tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(test_unit_db
    COMMAND $<TARGET_FILE:magic_folder_tests> --gtest_filter="*StoreTest*:*InfoServiceTest*:*DeleteServiceTest*"
    DEPENDS magic_folder_tests
    COMMENT "Running all database layer unit tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
) 