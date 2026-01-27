#[[
  File: cmake/pipeline/lint/lint-cpp-odpcpp.cmake
  Brief: Run OneDefinitionPerCppFile (ODPCPP) on src/**/*.cpp
]]
include_guard(GLOBAL)

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_collect_cpp_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_detect_bin_dir.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_find_or_build_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_plugin_base_args.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_filter_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin_on_list.cmake")

function(lint_cpp_odpcpp)
  _crsce_lint_cpp_detect_bin_dir(_BIN_DIR)

  # Collect candidate .cpp files (changed-only by default)
  _crsce_lint_collect_cpp_files(CPP_SOURCE_FILES)

  # Filter to only src/**/*.cpp
  _crsce_lint_cpp_filter_files(ODPCPP_SOURCE_FILES "^src/.*\\.cpp$" ${CPP_SOURCE_FILES})
  list(LENGTH ODPCPP_SOURCE_FILES _ODPCPP_LEN)
  message(STATUS "C++: found ${_ODPCPP_LEN} src files for ODPCPP")
  if (_ODPCPP_LEN EQUAL 0)
    message(STATUS "C++: no src/**/*.cpp files; skipping ODPCPP 😬 🤷 😬")
    return()
  endif ()

  _crsce_lint_cpp_run_plugin_on_list(
    ${_BIN_DIR}
    "${ODPCPP_SOURCE_FILES}"
    OneDefinitionPerCppFile
    OneDefinitionPerCppFile
  )

  message(STATUS "    ✅ C++ Lint (ODPCPP) passed")
endfunction()
