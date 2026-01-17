# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/compress.cmake

add_executable(compress cmd/compress/main.cpp)

target_include_directories(compress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src/common"
    "${PROJECT_SOURCE_DIR}/src/Compress"
)

# Add source files from src/Compress and src/common
file(GLOB_RECURSE COMPRESS_SOURCES
    "src/Compress/*.cpp"
    "src/common/*.cpp"
)

target_sources(compress PRIVATE ${COMPRESS_SOURCES})
