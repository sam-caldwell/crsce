# cmake/tools/ccache.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

# --- ccache integration (optional) ---
option(CRSCE_USE_CCACHE "Use ccache to speed up C/C++ compilation" ON)
if (CRSCE_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if (CCACHE_PROGRAM)
    # Use a repo-local cache so CI and developers share a predictable path
    set(_CCACHE_DIR "${PROJECT_SOURCE_DIR}/.ccache")
    file(MAKE_DIRECTORY "${_CCACHE_DIR}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM} (CCACHE_DIR=${_CCACHE_DIR})")
    # Inject CCACHE_DIR via a launcher wrapper so the build tool inherits it reliably
    set(_CCACHE_LAUNCH ${CMAKE_COMMAND} -E env CCACHE_DIR=${_CCACHE_DIR} CCACHE_BASEDIR=${PROJECT_SOURCE_DIR} ${CCACHE_PROGRAM})
    set(CMAKE_C_COMPILER_LAUNCHER   ${_CCACHE_LAUNCH})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${_CCACHE_LAUNCH})
  else ()
    message(STATUS "ccache not found; proceeding without compiler launcher")
  endif ()
endif ()
