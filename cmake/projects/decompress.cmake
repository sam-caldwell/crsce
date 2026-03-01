# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/decompress.cmake

add_executable(decompress cmd/decompress/main.cpp)

target_include_directories(decompress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Decompress CLI sources + shared common sources
set(DECOMPRESS_CLI_SOURCES
    src/Decompress/Cli/run.cpp
    src/Decompress/Cli/Heartbeat.cpp
    src/Decompress/Cli/detail/heartbeat_worker.cpp
)
file(GLOB_RECURSE DECOMPRESS_COMMON_SOURCES CONFIGURE_DEPENDS
    "src/common/*.cpp"
)
target_sources(decompress PRIVATE ${DECOMPRESS_CLI_SOURCES} ${DECOMPRESS_COMMON_SOURCES})

# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET decompress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:decompress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
