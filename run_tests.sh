#!/bin/bash

# Magic Folder Test Runner
# This script builds the project with testing enabled and runs the tests

set -e  # Exit on any error

echo "=== Magic Folder Test Runner ==="

# Check for VCPKG_ROOT
if [ -z "$VCPKG_ROOT" ]; then
    echo "Warning: VCPKG_ROOT not set. Please set it to enable testing with Google Test."
    echo "You can install vcpkg and set VCPKG_ROOT to the installation directory."
    exit 1
fi

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the magic-folder directory."
    exit 1
fi

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi
cd build

echo "Running all tests..."
if [ -f "bin/magic_folder_tests" ]; then
    echo "Running Magic Folder Test Suite..."
    # Enable Google Test colors and verbose output
    GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_color=yes
    echo ""
    echo "Test run completed successfully!"
    echo ""
    echo "Available test filtering options:"
    echo "  Run all tests:                   GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_color=yes"
    echo "  Run FileInfoService tests only:  GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter=\"FileInfoServiceTest.*\" --gtest_color=yes"
    echo "  Run FileDeleteService tests only: GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter=\"FileDeleteServiceTest.*\" --gtest_color=yes"
    echo "  Run FileProcessingService tests only: GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_filter=\"FileProcessingServiceTest.*\" --gtest_color=yes"
    echo "  List all available tests:        ./bin/magic_folder_tests --gtest_list_tests"
    echo "  Run with verbose output:         GTEST_COLOR=yes ./bin/magic_folder_tests --gtest_output=verbose --gtest_color=yes"
elif command -v ctest &> /dev/null; then
    echo "Using CTest to run tests..."
    ctest --output-on-failure --verbose
else
    echo "Error: Neither magic_folder_tests executable nor ctest found."
    echo "Build may have failed or tests may not be properly configured."
    exit 1
fi

echo "=== Test run complete ===" 