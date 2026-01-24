#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_phase_clang_tidy.cmake
  Brief: Run clang-tidy over provided sources.
]]
include_guard(GLOBAL)

function(_crsce_lint_cpp_phase_clang_tidy BIN_DIR CLANG_TIDY_EXE)
  # Remaining args are the source files (true CMake list from caller).
  set(SRC_LIST_RAW ${ARGN})

  # Normalize to absolute paths to make execution independent of CWD.
  set(SRC_LIST "")
  foreach(_f IN LISTS SRC_LIST_RAW)
    get_filename_component(_abs "${_f}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    list(APPEND SRC_LIST "${_abs}")
  endforeach()

  include("${CMAKE_SOURCE_DIR}/cmake/pipeline/sdk.cmake")

  set(_EXTRA_ARGS "")
  set(_RES_DIR "")

  crsce_sdk_env_prefix(_ENV_PREFIX)
  crsce_sdk_tidy_extra_args(_EXTRA_ARGS_LIST)
  if (_EXTRA_ARGS_LIST)
    set(_EXTRA_ARGS ${_EXTRA_ARGS_LIST})
  endif ()

  execute_process(
          COMMAND /opt/homebrew/opt/llvm/bin/clang++ -print-resource-dir
          OUTPUT_VARIABLE _RES_DIR
          OUTPUT_STRIP_TRAILING_WHITESPACE
          RESULT_VARIABLE _rd_rc
  )
  if (_rd_rc EQUAL 0 AND _RES_DIR)
    list(APPEND _EXTRA_ARGS "-extra-arg=-resource-dir=${_RES_DIR}")
  endif ()

  # Build a shell command string (because _run uses bash -lc).
  set(WARN_AS_ERR "-warnings-as-errors=*")
  # Escape header-filter so bash does not interpret parentheses when invoked via -lc
  set(HEADER_FILTER "-header-filter=\"^(include|src|cmd|test)/\"")
  string(JOIN " " _EXTRA_STR ${_EXTRA_ARGS})
  set(_BASE_STR "${CLANG_TIDY_EXE} -p ${BIN_DIR} ${WARN_AS_ERR} ${HEADER_FILTER} ${_EXTRA_STR}")

  # Write one file per line.
  string(JOIN "\n" _SRC_NL ${SRC_LIST})
  set(_LIST_FILE "${BIN_DIR}/.clang_tidy_files.txt")
  file(WRITE "${_LIST_FILE}" "${_SRC_NL}\n")

  # Note: plain xargs splits on whitespace; this assumes paths don’t contain spaces.
  _run("clang-tidy (parallel)"
          bash -lc "xargs -P ${_NPROC} -n 1 ${_BASE_STR} < '${_LIST_FILE}'")

endfunction()
