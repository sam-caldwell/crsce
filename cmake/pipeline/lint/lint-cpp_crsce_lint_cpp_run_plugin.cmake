#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_run_plugin.cmake
  Brief: Run a generic clang plugin over a file list with optional skip patterns.
]]
include_guard(GLOBAL)

function(_crsce_lint_cpp_run_plugin BIN_DIR FILES PLUGIN_DIR_NAME PLUGIN_FLAG)
  set(_SKIP_PATTERNS ${ARGN})

  _crsce_lint_cpp_find_or_build_plugin(${PLUGIN_DIR_NAME} _PLUG)

  _crsce_lint_cpp_plugin_base_args(${BIN_DIR} _BASE_STR)
  foreach (_P IN LISTS FILES)
    if (NOT IS_ABSOLUTE "${_P}")
      set(_FP "${CMAKE_SOURCE_DIR}/${_P}")
    else ()
      set(_FP "${_P}")
    endif ()

    if (NOT EXISTS "${_FP}")
      message(STATUS "  😬 🤷 😬 File skipped (not found): ${_FP}")
      continue()
    endif ()

    set(_SKIP_THIS FALSE)
    foreach (_RX IN LISTS _SKIP_PATTERNS)
      if (_FP MATCHES "${_RX}")
        set(_SKIP_THIS TRUE)
      endif ()
    endforeach ()
    if (_SKIP_THIS)
      continue()
    endif ()

    set(_CMD "${_BASE_STR} -Xclang -load -Xclang \"${_PLUG}\" -Xclang -plugin -Xclang ${PLUGIN_FLAG} \"${_FP}\"")

    execute_process(COMMAND bash -lc "${_CMD}" OUTPUT_VARIABLE _OUT ERROR_VARIABLE _ERR RESULT_VARIABLE _RC)

    if (NOT _RC EQUAL 0)
      message(STATUS "  🔥${PLUGIN_FLAG} failed on ${_P} (${_RC}).\n${_OUT}${_ERR}")
      message(FATAL_ERROR "-- 🔥Linter failed 🔥 --")
    endif ()
    message(STATUS "  ✅ Lint passed for ${_P}")
  endforeach ()
endfunction()
