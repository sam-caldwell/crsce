#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_collect.cmake
  Brief: Collect candidate C++ sources and partition by path.
]]
include_guard(GLOBAL)

# return the list of changed .cpp files
function(_crsce_lint_collect_cpp_files out)

  _git_changed_files(_SRC_CPP_LIST -- "*.cpp")

  set(${out} ${_SRC_CPP_LIST} PARENT_SCOPE)

  list(LENGTH _SRC_CPP_LIST _n)

  message(STATUS "Found ${_n} files")

endfunction()
