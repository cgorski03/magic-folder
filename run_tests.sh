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

EXTRA_CMAKE_ARGS=""
# Check if the operating system is macOS (Darwin kernel)
if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "macOS detected. Adding vcpkg arguments to allow unsupported builds."
    EXTRA_CMAKE_ARGS="-DVCPKG_INSTALL_OPTIONS=--allow-unsupported"
fi

cd build

echo "Installing dependencies with testing feature..."
"$VCPKG_ROOT/vcpkg" install --feature-flags=manifests,versions --feature testing

echo "Configuring CMake with testing enabled..."
cmake -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DCMAKE_BUILD_TYPE=Debug \
      $EXTRA_CMAKE_ARGS \
      ..

echo "Building project with tests..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Running all tests..."
if [ -f "bin/magic_folder_tests" ]; then
    echo "Running Magic Folder Test Suite..."
    ./bin/magic_folder_tests
    echo ""
    echo "Test run completed successfully!"
    echo ""
    echo "Available test filtering options:"
    echo "  Run all tests:                   ./bin/magic_folder_tests"
    echo "  Run FileInfoService tests only:  ./bin/magic_folder_tests --gtest_filter=\"FileInfoServiceTest.*\""
    echo "  List all available tests:        ./bin/magic_folder_tests --gtest_list_tests"
    echo "  Run with verbose output:         ./bin/magic_folder_tests --gtest_output=verbose"
elif command -v ctest &> /dev/null; then
    echo "Using CTest to run tests..."
    ctest --output-on-failure --verbose
else
    echo "Error: Neither magic_folder_tests executable nor ctest found."
    echo "Build may have failed or tests may not be properly configured."
    exit 1
fi

echo "=== Test run complete ===" 