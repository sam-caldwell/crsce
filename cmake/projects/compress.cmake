# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/compress.cmake

add_executable(compress cmd/compress/main.cpp)

target_include_directories(compress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Compress CLI sources + Compressor sources + shared common + solver sources
set(COMPRESS_CLI_SOURCES
    src/Compress/Cli/run.cpp
    src/Compress/Cli/Heartbeat.cpp
    src/Compress/Cli/heartbeat_worker.cpp
)
file(GLOB_RECURSE COMPRESS_COMPRESSOR_SOURCES CONFIGURE_DEPENDS
    "src/Compress/Compressor/*.cpp"
)
file(GLOB_RECURSE COMPRESS_COMMON_SOURCES CONFIGURE_DEPENDS
    "src/common/*.cpp"
)
# Compressor needs the solver for DI discovery
file(GLOB_RECURSE COMPRESS_SOLVER_SOURCES CONFIGURE_DEPENDS
    "src/Decompress/Solvers/*.cpp"
)

# Discover assembly sources (.S files) for hardware-accelerated SHA
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    file(GLOB_RECURSE COMPRESS_ASM_SOURCES CONFIGURE_DEPENDS
        "src/common/*_arm64.S"
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    file(GLOB_RECURSE COMPRESS_ASM_SOURCES CONFIGURE_DEPENDS
        "src/common/*_amd64.S"
    )
endif()

# Discover Objective-C++ solver sources when Metal is enabled
if(CRSCE_ENABLE_METAL)
    file(GLOB_RECURSE COMPRESS_SOLVER_OBJCXX_SOURCES CONFIGURE_DEPENDS
        "src/Decompress/Solvers/*.mm"
    )
endif()

target_sources(compress PRIVATE
    ${COMPRESS_CLI_SOURCES}
    ${COMPRESS_COMPRESSOR_SOURCES}
    ${COMPRESS_COMMON_SOURCES}
    ${COMPRESS_SOLVER_SOURCES}
    ${COMPRESS_ASM_SOURCES}
    ${COMPRESS_SOLVER_OBJCXX_SOURCES}
)

# Metal GPU acceleration
if(CRSCE_ENABLE_METAL)
    target_compile_definitions(compress PRIVATE CRSCE_ENABLE_METAL=1)
    target_link_libraries(compress PRIVATE "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
    add_dependencies(compress metal_shaders)
endif()

# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET compress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:compress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
