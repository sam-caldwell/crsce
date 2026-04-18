# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/decompress.cmake

add_executable(decompress cmd/decompress/main.cpp)

target_include_directories(decompress PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Decompress CLI sources + Decompressor + Solvers + shared common sources
set(DECOMPRESS_CLI_SOURCES
    src/Decompress/Cli/run.cpp
    src/Decompress/Cli/Heartbeat.cpp
    src/Decompress/Cli/heartbeat_worker.cpp
)
file(GLOB_RECURSE DECOMPRESS_DECOMPRESSOR_SOURCES CONFIGURE_DEPENDS
    "src/Decompress/Decompressor/*.cpp"
)
file(GLOB_RECURSE DECOMPRESS_SOLVER_SOURCES CONFIGURE_DEPENDS
    "src/Decompress/Solvers/*.cpp"
)
file(GLOB_RECURSE DECOMPRESS_COMMON_SOURCES CONFIGURE_DEPENDS
    "src/common/*.cpp"
)

# Discover assembly sources (.S files) for hardware-accelerated SHA
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    file(GLOB_RECURSE DECOMPRESS_ASM_SOURCES CONFIGURE_DEPENDS
        "src/common/*_arm64.S"
    )
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    file(GLOB_RECURSE DECOMPRESS_ASM_SOURCES CONFIGURE_DEPENDS
        "src/common/*_amd64.S"
    )
endif()

# Discover Objective-C++ solver sources when Metal is enabled
if(CRSCE_ENABLE_METAL)
    file(GLOB_RECURSE DECOMPRESS_SOLVER_OBJCXX_SOURCES CONFIGURE_DEPENDS
        "src/Decompress/Solvers/*.mm"
    )
endif()

target_sources(decompress PRIVATE
    ${DECOMPRESS_CLI_SOURCES}
    ${DECOMPRESS_DECOMPRESSOR_SOURCES}
    ${DECOMPRESS_SOLVER_SOURCES}
    ${DECOMPRESS_COMMON_SOURCES}
    ${DECOMPRESS_ASM_SOURCES}
    ${DECOMPRESS_SOLVER_OBJCXX_SOURCES}
)

# Metal GPU acceleration
if(CRSCE_ENABLE_METAL)
    target_compile_definitions(decompress PRIVATE CRSCE_ENABLE_METAL=1)
    target_link_libraries(decompress PRIVATE "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
    add_dependencies(decompress metal_shaders)
endif()

# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET decompress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:decompress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
