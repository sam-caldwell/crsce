# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/compress.cmake

add_executable(compress cmd/compress/main.cpp)

target_include_directories(compress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Compress CLI sources + shared common sources
set(COMPRESS_CLI_SOURCES
    src/Compress/Cli/run.cpp
    src/Compress/Cli/Heartbeat.cpp
    src/Compress/Cli/detail/heartbeat_worker.cpp
)
file(GLOB_RECURSE COMPRESS_COMMON_SOURCES CONFIGURE_DEPENDS
    "src/common/*.cpp"
)
target_sources(compress PRIVATE ${COMPRESS_CLI_SOURCES} ${COMPRESS_COMMON_SOURCES})

# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET compress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:compress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
