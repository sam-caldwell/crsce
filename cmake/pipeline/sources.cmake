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

# Also provide a static library for tools/tests to link, which improves coverage attribution
add_library(crsce_static STATIC ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_static PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)
