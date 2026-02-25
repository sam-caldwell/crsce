# cmake/pipeline/sources.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Centralized source discovery for project build graph.
include_guard(GLOBAL)

# Discover and centralize project sources
file(GLOB_RECURSE CRSCE_PROJECT_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

add_library(crsce_sources OBJECT ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_sources PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions(crsce_sources PUBLIC
  CRSCE_BIN_DIR="${PROJECT_SOURCE_DIR}/bin"
)
option(CRSCE_SOLVER_CONSTRAINT "Build with constraint solver (sources)" ON)
target_compile_definitions(crsce_sources PUBLIC CRSCE_SOLVER_CONSTRAINT)

# If metal-cpp headers are present at the default path, expose includes for sources target only.
# ObjC++/Metal enablement is handled per-target (decompress) to avoid global framework deps.

# Also provide a static library for tools/tests to link, which improves coverage attribution
add_library(crsce_static STATIC ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_static PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)
# Disable clang-tidy on Metal interop file to avoid vendor/header-cleaner noise
set_source_files_properties(
  "${PROJECT_SOURCE_DIR}/src/Gpu/Metal/RectScorerMetal.cpp"
  PROPERTIES SKIP_CLANG_TIDY ON
)
target_compile_definitions(crsce_static PUBLIC
  CRSCE_BIN_DIR="${PROJECT_SOURCE_DIR}/bin"
)
target_compile_definitions(crsce_static PUBLIC CRSCE_SOLVER_CONSTRAINT)

# Note: crsce_static does not define CRSCE_HAS_METAL; GPU interop compiles only in targets that enable it explicitly.
