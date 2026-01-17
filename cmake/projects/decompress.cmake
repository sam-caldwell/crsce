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
