# cmake/pipeline/sources.cmake
include_guard(GLOBAL)

# Discover and centralize project sources
file(GLOB_RECURSE CRSCE_PROJECT_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

add_library(crsce_sources OBJECT ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_sources PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)

