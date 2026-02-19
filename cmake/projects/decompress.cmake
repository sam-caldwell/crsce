# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/decompress.cmake

add_executable(decompress cmd/decompress/main.cpp)

target_include_directories(decompress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/common"
    "${PROJECT_SOURCE_DIR}/src/Decompress"
)

# Add source files from src/Decompress and src/common
file(GLOB_RECURSE DECOMPRESS_SOURCES
    "src/Decompress/*.cpp"
    "src/common/*.cpp"
)

target_sources(decompress PRIVATE ${DECOMPRESS_SOURCES})

# Solver selection (single primary).
#
# If none is defined, the build fails via SelectedSolver.h.
# We define only CRSCE_SOLVER_PRIMARY_GOBP here.
target_compile_definitions(decompress PUBLIC CRSCE_SOLVER_PRIMARY_GOBP)


# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET decompress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:decompress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
