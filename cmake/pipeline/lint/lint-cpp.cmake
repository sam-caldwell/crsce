#[[
  File: cmake/pipeline/lint/lint-cpp.cmake
  Brief: Lint all C++ files
]]
include_guard(GLOBAL)

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_collect_cpp_files.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_detect_bin_dir.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_phase_clang_tidy.cmake")

# Breakout plugin passes
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp-otpf.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp-odpcpp.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp-odph.cmake")

function(lint_cpp)
  _crsce_lint_cpp_detect_bin_dir(_BIN_DIR)
  # we will have failed if BIN_DIR is empty

  find_program(CLANG_TIDY_EXE clang-tidy)
  if (NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR " clang-tidy is required. Run 'make ready/fix' to install llvm.")
  endif ()

  _crsce_lint_collect_cpp_files(CPP_SOURCE_FILES)

  # Treat as an actual list; skip cleanly if empty.
  list(LENGTH CPP_SOURCE_FILES _CPP_LEN)
  if (_CPP_LEN EQUAL 0)
    message(STATUS "C++: no candidate sources (tests); skipping clang-tidy 😬 🤷 😬")
    return()
  endif ()

  # Exclude known intentional anti-pattern fixtures from clang-tidy.
  set(TIDY_FILES ${CPP_SOURCE_FILES})
  include("${CMAKE_SOURCE_DIR}/cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_filter_files.cmake")
  _crsce_lint_cpp_filter_files(_EXCLUDE_FIXTURES "^test/tools/clang-plugins/.*/fixtures/.*\\.cpp$" ${CPP_SOURCE_FILES})
  if (_EXCLUDE_FIXTURES)
    list(REMOVE_ITEM TIDY_FILES ${_EXCLUDE_FIXTURES})
  endif ()

  # IMPORTANT: do not quote the list unless the callee explicitly expects a single string.
  _crsce_lint_cpp_phase_clang_tidy(${_BIN_DIR} ${CLANG_TIDY_EXE} ${TIDY_FILES})

  # Run plugin passes
  lint_cpp_otpf()
  lint_cpp_odpcpp()
  lint_cpp_odph()
  message(STATUS "  ✅ C++ Lint passed")
endfunction()
