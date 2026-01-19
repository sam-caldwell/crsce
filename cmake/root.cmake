# cmake/root.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

include(cmake/pipeline/build.cmake)
include(cmake/pipeline/clean.cmake)
include(cmake/pipeline/configure.cmake)

# --- macOS SDK (Homebrew LLVM) auto-detect to keep toolchain consistent
include(cmake/pipeline/sdk.cmake)

# --- Optimization defaults by configuration ---
# Ensure optimized builds use -O3 (Release) and -O2 (RelWithDebInfo).
# Debug remains unoptimized for easier debugging and coverage instrumentation.
add_compile_options(
  $<$<CONFIG:Release>:-O3>
  $<$<CONFIG:RelWithDebInfo>:-O2>
)


include(cmake/apple.cmake)

# --- Include project definitions ---
include(cmake/projects/compress.cmake)
include(cmake/projects/decompress.cmake)
include(cmake/projects/hello_world.cmake)
include(cmake/projects/hasher.cmake)
include(cmake/projects/test_hasher_helpers.cmake)
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
    COMMAND ${CLANG_TIDY_EXE} -p=${CMAKE_BINARY_DIR} "-header-filter=.*" --extra-arg=-w --extra-arg=-fdiagnostics-show-option --extra-arg=-fcolor-diagnostics ${ALL_CXX_SOURCES}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )
endif()


# --- Testing ---
include(cmake/pipeline/test.cmake)
enable_testing()

# Trigger CMake reconfigure when any src/*.cpp changes (recursive)
file(GLOB_RECURSE PROJECT_SRC_CPP CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")
