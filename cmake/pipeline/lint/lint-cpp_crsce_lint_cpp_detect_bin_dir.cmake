#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_detect_bin_dir.cmake
  Brief: Helper to detect compile_commands.json root.
]]
include_guard(GLOBAL)

function(_crsce_lint_cpp_detect_bin_dir out)

  if (DEFINED LINT_BUILD_DIR)
    set(_BIN_DIR "${LINT_BUILD_DIR}")
  else ()
    set(_BIN_DIR "${CMAKE_SOURCE_DIR}/build/llvm-debug")
  endif ()

  if (NOT EXISTS "${_BIN_DIR}/compile_commands.json")

    set(_ALT_DIR "${CMAKE_SOURCE_DIR}/build/arm64-debug")
    if (EXISTS "${_ALT_DIR}/compile_commands.json")

      set(_BIN_DIR "${_ALT_DIR}")

    else ()

      file(GLOB CC_JSON_FILES "${CMAKE_SOURCE_DIR}/build/*/compile_commands.json")
      list(LENGTH CC_JSON_FILES _CC_LEN)
      if (_CC_LEN GREATER 0)
        list(GET CC_JSON_FILES 0 _CC_PATH)
        get_filename_component(_BIN_DIR "${_CC_PATH}" DIRECTORY)
      endif ()

    endif ()
  endif ()

  set(${out} "${_BIN_DIR}" PARENT_SCOPE)

  if (NOT EXISTS "${_BIN_DIR}/compile_commands.json")
    message(FATAL_ERROR "🔥 clang-tidy compile database missing at '${_BIN_DIR}'. Run 'make configure'.")
  endif ()

endfunction()
