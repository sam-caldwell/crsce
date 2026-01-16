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

file(GLOB_RECURSE ALL_CXX_SOURCES
  "${PROJECT_SOURCE_DIR}/cmd/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/common/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/Compress/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/Decompress/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*.cpp"
)

if(CMAKE_CXX_CLANG_TIDY)
  add_custom_target(clang-tidy
    COMMAND "${CMAKE_CXX_CLANG_TIDY}"
            "--header-filter=^(${PROJECT_SOURCE_DIR}/(include|src|test)/.*)"
            "-p=${CMAKE_BINARY_DIR}"
            ${ALL_CXX_SOURCES}
    VERBATIM
  )
endif()
