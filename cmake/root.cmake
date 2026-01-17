# cmake/root.cmake

include(cmake/pipeline/build.cmake)
include(cmake/pipeline/clean.cmake)
include(cmake/pipeline/configure.cmake)
include(cmake/pipeline/cover.cmake)
include(cmake/pipeline/lint.cmake)

# --- Include project definitions ---
include(cmake/projects/compress.cmake)
include(cmake/projects/decompress.cmake)
include(cmake/projects/hello_world.cmake)
include(cmake/pipeline/sources.cmake)

# --- Testing ---
include(cmake/pipeline/test.cmake)
enable_testing()

# Trigger CMake reconfigure when any src/*.cpp changes (recursive)
file(GLOB_RECURSE PROJECT_SRC_CPP CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")

# (No C++ linter integration is configured here.)
