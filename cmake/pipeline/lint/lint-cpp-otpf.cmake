#[[
  File: cmake/pipeline/lint/lint-cpp-otpf.cmake
  Brief: Run OneTestPerFile (OTPF) on test/**/*.cpp
]]
include_guard(GLOBAL)

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_collect_cpp_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_detect_bin_dir.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_find_or_build_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_plugin_base_args.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_filter_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin_on_list.cmake")

function(lint_cpp_otpf)
  _crsce_lint_cpp_detect_bin_dir(_BIN_DIR)

  # Collect candidate .cpp files (changed-only by default)
  _crsce_lint_collect_cpp_files(CPP_SOURCE_FILES)

  # Filter to only test/**/*.cpp
  message(STATUS "C++: Filtering for test files (OTPF)")
  _crsce_lint_cpp_filter_files(TEST_CPP_SOURCE_FILES "^test/.*\\.cpp$" ${CPP_SOURCE_FILES})
  list(LENGTH TEST_CPP_SOURCE_FILES _TEST_LEN)
  message(STATUS "C++: found ${_TEST_LEN} test files for OTPF")
  if (_TEST_LEN EQUAL 0)
    message(STATUS "C++: no test/**/*.cpp files; skipping OTPF 😬 🤷 😬")
    return()
  endif ()

  # Exclude known helpers and all fixtures
  _crsce_lint_cpp_run_plugin_on_list(
    ${_BIN_DIR}
    "${TEST_CPP_SOURCE_FILES}"
    OneTestPerFile
    OneTestPerFile
    "/test/cmd/hasher/helpers/.*\\.cpp$"
    "/test/.*/fixtures/.*\\.cpp$"
  )

  message(STATUS "    ✅ C++ Lint (OTPF) passed")
endfunction()
