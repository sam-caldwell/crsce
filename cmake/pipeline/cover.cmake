#[[
  File: cmake/pipeline/cover.cmake
  Brief: Orchestrates source-based coverage using modular includes.

  Usage:
    cmake -D BUILD_DIR=<build_root> -D SOURCE_DIR=<repo_root> -P cmake/pipeline/cover.cmake
]]

# Initialize environment and toolchain
include(${CMAKE_CURRENT_LIST_DIR}/cover/init.cmake)

# Configure and build instrumented binaries
include(${CMAKE_CURRENT_LIST_DIR}/cover/configure.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cover/build.cmake)

# Run tests with profiling and merge raw profiles
include(${CMAKE_CURRENT_LIST_DIR}/cover/run_tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cover/merge_profiles.cmake)

# Discover binaries and generate coverage report with threshold enforcement
include(${CMAKE_CURRENT_LIST_DIR}/cover/find_binaries.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cover/report.cmake)
