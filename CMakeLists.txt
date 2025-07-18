cmake_minimum_required(VERSION 3.20)
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()
project(magic-folder VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/ollama_hpp/include)

# ---------- macOS + Homebrew OpenMP ----------
if(APPLE)
    set(OpenMP_C_FLAGS          "-Xpreprocessor -fopenmp" CACHE STRING "")
    set(OpenMP_CXX_FLAGS        "-Xpreprocessor -fopenmp" CACHE STRING "")
    set(OpenMP_C_LIB_NAMES      "omp" CACHE STRING "")
    set(OpenMP_CXX_LIB_NAMES    "omp" CACHE STRING "")
    set(OpenMP_omp_LIBRARY      "/opt/homebrew/opt/libomp/lib/libomp.dylib" CACHE FILEPATH "")
    include_directories(SYSTEM /opt/homebrew/opt/libomp/include)
    link_directories(/opt/homebrew/opt/libomp/lib)
endif()

find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(LAPACK REQUIRED)
find_package(faiss CONFIG REQUIRED)

# Add subdirectories
add_subdirectory(src/magic_core)
add_subdirectory(src/magic_api)
add_subdirectory(src/magic_cli)
add_subdirectory(src/magic_services)

# Enable testing
enable_testing()
add_subdirectory(tests) 