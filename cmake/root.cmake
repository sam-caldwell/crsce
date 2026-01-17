# cmake/root.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

include(cmake/pipeline/build.cmake)
include(cmake/pipeline/clean.cmake)
include(cmake/pipeline/configure.cmake)
include(cmake/pipeline/lint.cmake)

# --- Include project definitions ---
include(cmake/projects/compress.cmake)
include(cmake/projects/decompress.cmake)
include(cmake/projects/hello_world.cmake)
include(cmake/pipeline/sources.cmake)

# --- clang-tidy integration (optional) ---
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  file(GLOB_RECURSE ALL_CXX_SOURCES
    "${PROJECT_SOURCE_DIR}/cmd/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/test/*.cpp"
  )
  add_custom_target(clang-tidy
    COMMAND ${CLANG_TIDY_EXE} -p=${CMAKE_BINARY_DIR} --quiet ${ALL_CXX_SOURCES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )
endif()

# --- Testing ---
include(cmake/pipeline/test.cmake)
enable_testing()

# Trigger CMake reconfigure when any src/*.cpp changes (recursive)
file(GLOB_RECURSE PROJECT_SRC_CPP CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")

# (No C++ linter integration is configured here.)
