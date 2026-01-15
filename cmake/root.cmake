# cmake/root.cmake

# --- Include project definitions ---
include(cmake/projects/compress.cmake)
include(cmake/projects/decompress.cmake)

# --- Testing ---
enable_testing()

# Add the test directory
add_subdirectory(test)

# --- Linter ---
# Find clang-tidy
find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
else()
  message(STATUS "clang-tidy not found.")
endif()