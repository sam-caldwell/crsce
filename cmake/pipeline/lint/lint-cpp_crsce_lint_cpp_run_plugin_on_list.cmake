#[[
  File: cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin_on_list.cmake
  Brief: Lint all C++ files
]]
include_guard(GLOBAL)

# Wrapper to make it obvious we're intentionally passing the list as ONE argument (a ;-separated string).
function(_crsce_lint_cpp_run_plugin_on_list BIN_DIR FILE_LIST PLUGIN_TARGET PLUGIN_NAME)
  _crsce_lint_cpp_run_plugin(
          ${BIN_DIR}
          "${FILE_LIST}"
          ${PLUGIN_TARGET}
          ${PLUGIN_NAME}
          ${ARGN})
endfunction()
