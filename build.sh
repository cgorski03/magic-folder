# Magic Folder C++ Build Script

set -e

echo "Building Magic Folder C++..."

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

# Navigate to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
if [ -n "$VCPKG_ROOT" ]; then
    cmake -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ..
else
    cmake ..
fi

# Build the project
echo "Building project..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Build completed successfully!"
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