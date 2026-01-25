#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_collect_h_files.cmake
  Brief: Collect candidate header files under include/ for ODPH.
]]
include_guard(GLOBAL)

function(_crsce_lint_collect_h_files out)
  if (LINT_CHANGED_ONLY)
    _git_changed_files(_CHANGED_RAW -- "*.h")
    set(_H_LIST "")
    foreach(_f IN LISTS _CHANGED_RAW)
      if(_f MATCHES "^include/.*\\.h$")
        list(APPEND _H_LIST "${_f}")
      endif()
    endforeach()
  else ()
    file(GLOB_RECURSE _H_LIST
      "${CMAKE_SOURCE_DIR}/include/**/*.h"
    )
  endif ()

  set(${out} ${_H_LIST} PARENT_SCOPE)
  list(LENGTH _H_LIST _n)
  if (LINT_CHANGED_ONLY)
    message(STATUS "Headers: changed .h files under include/: ${_n}")
  else ()
    message(STATUS "Headers: all .h files under include/: ${_n}")
  endif ()
endfunction()
