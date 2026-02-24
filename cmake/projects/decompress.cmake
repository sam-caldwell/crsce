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

# Provide bin dir macro to any sources (e.g., Metal interop) that reference it.
target_compile_definitions(decompress PUBLIC
  CRSCE_BIN_DIR="${PROJECT_SOURCE_DIR}/bin"
)

# Add source files from src/Decompress and src/common
file(GLOB_RECURSE DECOMPRESS_SOURCES
    "src/Decompress/*.cpp"
    "src/common/*.cpp"
    "src/Gpu/*.cpp"
)

target_sources(decompress PRIVATE ${DECOMPRESS_SOURCES})

# Solver selection (compile-time)
# Default to the constraint-based solver for all normal builds unless explicitly overridden.
option(CRSCE_SOLVER_CONSTRAINT "Build decompressor with constraint solver" ON)
if(CRSCE_SOLVER_CONSTRAINT)
  target_compile_definitions(decompress PUBLIC CRSCE_SOLVER_CONSTRAINT)
else()
  target_compile_definitions(decompress PUBLIC CRSCE_SOLVER_PIPELINE)
endif()

# Apple Metal acceleration via ObjC++ shim (optional; OFF by default)
if(APPLE)
  option(CRSCE_ENABLE_METAL "Enable Apple Metal acceleration for constraint solver" OFF)
  if(CRSCE_ENABLE_METAL)
    # Enable GPU path when frameworks are available; ObjC++ compiler is configured separately via presets.
    find_library(METAL_FRAMEWORK Metal)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(OBJC_LIBRARY objc)
    if(METAL_FRAMEWORK AND FOUNDATION_FRAMEWORK)
      target_compile_definitions(decompress PUBLIC CRSCE_HAS_METAL)
      # Add the ObjC++ shim implementation
      target_sources(decompress PRIVATE "${PROJECT_SOURCE_DIR}/src/Gpu/Metal/RectScorerObjC.mm")
      # Enable ARC for ObjC++ sources in this target
      target_compile_options(decompress PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
      # Link frameworks and Objective-C runtime
      target_link_libraries(decompress PUBLIC "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
      if(OBJC_LIBRARY)
        target_link_libraries(decompress PUBLIC "${OBJC_LIBRARY}")
      endif()
    else()
      message(WARNING "Metal or Foundation framework not found; GPU acceleration disabled.")
    endif()

    # Skip clang-tidy for the C++ wrapper TU to avoid SDK/vendor header issues
    set_source_files_properties(
      "${PROJECT_SOURCE_DIR}/src/Gpu/Metal/RectScorerMetal.cpp"
      PROPERTIES SKIP_CLANG_TIDY ON
    )
  endif()
endif()


# Stage binary to a unified bin/ directory for convenience
add_custom_command(TARGET decompress POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:decompress>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
