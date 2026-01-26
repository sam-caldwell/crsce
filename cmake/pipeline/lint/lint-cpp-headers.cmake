#[[
  File: cmake/pipeline/lint/lint-cpp-headers.cmake
  Brief: Run OneDefinitionPerHeader over include/**/*.h only, independent of clang-tidy.
]]
include_guard(GLOBAL)

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_detect_bin_dir.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_find_or_build_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_plugin_base_args.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin_on_list.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_collect_h_files.cmake")

function(lint_cpp_headers)
  _crsce_lint_cpp_detect_bin_dir(_BIN_DIR)

  _crsce_lint_collect_h_files(ODPH_HEADER_FILES)
  list(LENGTH ODPH_HEADER_FILES _ODPH_LEN)
  message(STATUS "Headers: found ${_ODPH_LEN} include headers for one-definition-per-header")
  if (_ODPH_LEN GREATER 0)
    _crsce_lint_cpp_run_plugin_on_list(
            ${_BIN_DIR}
            "${ODPH_HEADER_FILES}"
            OneDefinitionPerHeader
            one-definition-per-header)
  else ()
    message(STATUS "Headers: no include/**/*.h files; skipping ODPH 😬 🤷 😬")
  endif ()
endfunction()
