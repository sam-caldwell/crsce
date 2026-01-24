#[[
  File: cmake/pipeline/lint/lint-cpp.cmake
  Brief: Lint all C++ files
]]
include_guard(GLOBAL)

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_collect_cpp_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_detect_bin_dir.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_phase_clang_tidy.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_find_or_build_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_plugin_base_args.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_filter_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin_on_list.cmake")

function(lint_cpp)
  _crsce_lint_cpp_detect_bin_dir(_BIN_DIR)
  # we will have failed if BIN_DIR is empty

  find_program(CLANG_TIDY_EXE clang-tidy)
  if (NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "🔥 clang-tidy is required. Run 'make ready/fix' to install llvm.")
  endif ()

  _crsce_lint_collect_cpp_files(CPP_SOURCE_FILES)

  # Treat as an actual list; skip cleanly if empty.
  list(LENGTH CPP_SOURCE_FILES _CPP_LEN)
  if (_CPP_LEN EQUAL 0)
    message(STATUS "C++: no candidate sources (tests); skipping clang-tidy 😬 🤷 😬")
    return()
  endif ()

  # IMPORTANT: do not quote the list unless the callee explicitly expects a single string.
  _crsce_lint_cpp_phase_clang_tidy(${_BIN_DIR} ${CLANG_TIDY_EXE} ${CPP_SOURCE_FILES})

  # Run OneDefinitionPerCppFile only on src/**/*.cpp
  _crsce_lint_cpp_filter_files(ODPCPP_SOURCE_FILES "^src/.*\\.cpp$" ${CPP_SOURCE_FILES})
  list(LENGTH ODPCPP_SOURCE_FILES _ODPCPP_LEN)
  message(STATUS "C++: found ${_ODPCPP_LEN} src files for OneDefinitionPerCppFile")
  if (_ODPCPP_LEN GREATER 0)
    _crsce_lint_cpp_run_plugin_on_list(
            ${_BIN_DIR}
            "${ODPCPP_SOURCE_FILES}"
            OneDefinitionPerCppFile
            OneDefinitionPerCppFile)
  else ()
    message(STATUS "C++: no src/**/*.cpp files; skipping OneDefinitionPerCppFile plugin 😬 🤷 😬")
  endif ()

  # Filter to only test/**/*.cpp (relative to repo root).
  message(STATUS "C++: Filtering for test files")
  _crsce_lint_cpp_filter_files(TEST_CPP_SOURCE_FILES "^test/.*\\.cpp$" ${CPP_SOURCE_FILES})

  list(LENGTH TEST_CPP_SOURCE_FILES _TEST_LEN)
  message(STATUS "C++: found ${_TEST_LEN} test files to lint")
  if (_TEST_LEN EQUAL 0)
    message(STATUS "C++: no test/**/*.cpp files; skipping OneTestPerFile plugin  😬 🤷 😬")
  else ()
    _crsce_lint_cpp_run_plugin_on_list(
            ${_BIN_DIR}
            "${TEST_CPP_SOURCE_FILES}"
            OneTestPerFile
            OneTestPerFile
            "/test/cmd/hasher/helpers/.*\\.cpp$"
            "/test/tools/clang-plugins/.*/fixtures/.*\\.cpp$")
  endif ()

  message(STATUS "  ✅ C++ Lint passed")
endfunction()
