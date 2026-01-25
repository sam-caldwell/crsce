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
    _git_changed_files(_SRC_CPP_LIST -- "*.cpp")
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
