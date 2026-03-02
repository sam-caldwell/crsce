# cmake/pipeline/sources.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Centralized source discovery for project build graph.
include_guard(GLOBAL)

# Discover and centralize project sources
file(GLOB_RECURSE CRSCE_PROJECT_SOURCES CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# Discover Objective-C++ sources when Metal is enabled
if(CRSCE_ENABLE_METAL)
  file(GLOB_RECURSE CRSCE_OBJCXX_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.mm"
  )
  list(APPEND CRSCE_PROJECT_SOURCES ${CRSCE_OBJCXX_SOURCES})
endif()

add_library(crsce_sources OBJECT ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_sources PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions(crsce_sources PUBLIC
  CRSCE_BIN_DIR="${PROJECT_SOURCE_DIR}/bin"
  CRSCE_SRC_DIR="${PROJECT_SOURCE_DIR}/src"
  CRSCE_SOLVER_PIPELINE
)
if(CRSCE_ENABLE_METAL)
  target_compile_definitions(crsce_sources PUBLIC CRSCE_ENABLE_METAL=1)
  target_link_libraries(crsce_sources PUBLIC "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
endif()

# Also provide a static library for tools/tests to link, which improves coverage attribution
add_library(crsce_static STATIC ${CRSCE_PROJECT_SOURCES})
target_include_directories(crsce_static PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
)
target_compile_definitions(crsce_static PUBLIC
  CRSCE_BIN_DIR="${PROJECT_SOURCE_DIR}/bin"
  CRSCE_SRC_DIR="${PROJECT_SOURCE_DIR}/src"
  CRSCE_SOLVER_PIPELINE
)
if(CRSCE_ENABLE_METAL)
  target_compile_definitions(crsce_static PUBLIC CRSCE_ENABLE_METAL=1)
  target_link_libraries(crsce_static PUBLIC "${METAL_FRAMEWORK}" "${FOUNDATION_FRAMEWORK}")
endif()
