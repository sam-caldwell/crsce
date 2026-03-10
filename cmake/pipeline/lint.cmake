#[[
  File: cmake/pipeline/lint.cmake
  Brief: Root entry for lint pipeline. Dispatches to per-linter modules.

  Usage:
    cmake -D LINT_TARGET=all -P cmake/pipeline/lint.cmake
    cmake -D LINT_TARGET=ws|md|sh|py|make|cpp -P cmake/pipeline/lint.cmake
]]

if(NOT CMAKE_SCRIPT_MODE_FILE)
  return()
endif()

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/core.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-ws.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-md.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-sh.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-py.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-mk.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp.cmake")

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "ws")
  lint_ws()
endif()

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "md")
  lint_md()
endif()

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "sh")
  lint_sh()
endif()

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "py")
  lint_py()
endif()

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "make")
  lint_mk()
endif()

if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "cpp")
  lint_cpp()
endif()

message(STATUS "✅ Lint complete")
