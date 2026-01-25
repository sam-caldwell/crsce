#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_collect.cmake
  Brief: Collect candidate C++ sources and partition by path.
]]
include_guard(GLOBAL)

# Return the list of candidate .cpp files.
# - When LINT_CHANGED_ONLY is ON (default): limit to changed/tracked .cpp via git diff.
# - When LINT_CHANGED_ONLY is OFF: glob all .cpp under src/, cmd/, and test/.
function(_crsce_lint_collect_cpp_files out)
  if (LINT_CHANGED_ONLY)
    # Get changed .cpp files anywhere, then filter down to src/cmd/test only
    _git_changed_files(_CHANGED_RAW -- "*.cpp")
    set(_SRC_CPP_LIST "")
    foreach(_f IN LISTS _CHANGED_RAW)
      if(_f MATCHES "^src/.*\\.cpp$" OR _f MATCHES "^cmd/.*\\.cpp$" OR _f MATCHES "^test/.*\\.cpp$")
        list(APPEND _SRC_CPP_LIST "${_f}")
      endif()
    endforeach()
  else ()
    # In script mode, CONFIGURE_DEPENDS is not supported; perform a plain glob.
    file(GLOB_RECURSE _SRC_CPP_LIST
      "${CMAKE_SOURCE_DIR}/src/**/*.cpp"
      "${CMAKE_SOURCE_DIR}/cmd/**/*.cpp"
      "${CMAKE_SOURCE_DIR}/test/**/*.cpp"
    )
  endif ()

  set(${out} ${_SRC_CPP_LIST} PARENT_SCOPE)
  list(LENGTH _SRC_CPP_LIST _n)
  if (LINT_CHANGED_ONLY)
    message(STATUS "C++: changed .cpp files: ${_n}")
  else ()
    message(STATUS "C++: all .cpp files (src/cmd/test): ${_n}")
  endif ()
endfunction()
