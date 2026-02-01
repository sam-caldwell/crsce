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

# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET compress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:compress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
