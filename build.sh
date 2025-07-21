# Magic Folder C++ Build Script

set -e

echo "Building Magic Folder C++..."
BUILD_START_TIME=$(date +%s)

if [ -z "$VCPKG_ROOT" ]; then
    echo "Warning: VCPKG_ROOT not set. Falling back to system packages."
    echo "Consider installing vcpkg for better dependency management."
fi

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the magic-folder-cpp directory."
    exit 1
fi

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

EXTRA_CMAKE_ARGS=""
# Check if the operating system is macOS (Darwin kernel)
if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "macOS detected. Adding vcpkg arguments to allow unsupported builds (for Faiss)."
    # This sets a CMake variable that is passed to the vcpkg toolchain script.
    EXTRA_CMAKE_ARGS="-DVCPKG_INSTALL_OPTIONS=--allow-unsupported"
fi


# Navigate to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
CMAKE_START_TIME=$(date +%s)
if [ -n "$VCPKG_ROOT" ]; then
    cmake -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" $EXTRA_CMAKE_ARGS ..
else
    cmake ..
fi
CMAKE_END_TIME=$(date +%s)
CMAKE_DURATION=$((CMAKE_END_TIME - CMAKE_START_TIME))

# Build the project
echo "Building project..."
COMPILE_START_TIME=$(date +%s)
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
COMPILE_END_TIME=$(date +%s)
COMPILE_DURATION=$((COMPILE_END_TIME - COMPILE_START_TIME))

BUILD_END_TIME=$(date +%s)
TOTAL_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))

echo "Build completed successfully!"
echo ""
echo "=== Build Timing ==="
echo "CMake configuration: ${CMAKE_DURATION}s"
echo "Compilation time: ${COMPILE_DURATION}s"
echo "Total build time: ${TOTAL_DURATION}s"
echo ""
echo "Executables created:"
echo "  - ./build/bin/magic_api  (API server)"
echo "  - ./build/bin/magic_cli  (CLI tool)"
echo ""
echo "To run the API server:"
echo "  cd build && ./bin/magic_api"
echo ""
echo "To use the CLI:"
echo "  cd build && ./bin/magic_cli help" 