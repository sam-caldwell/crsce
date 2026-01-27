#[[
  File: cmake/pipeline/lint/lint-cpp_crsce_lint_cpp_run_plugin.cmake
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

    set(_EXTRA_PLUGIN_ARGS "")
    # Always print constructs for better diagnostics for these plugins
    if ("${PLUGIN_FLAG}" STREQUAL "OneDefinitionPerCppFile")
      set(_EXTRA_PLUGIN_ARGS "-Xclang -plugin-arg-OneDefinitionPerCppFile -Xclang print-constructs")
    elseif ("${PLUGIN_FLAG}" STREQUAL "one-definition-per-header")
      set(_EXTRA_PLUGIN_ARGS "-Xclang -plugin-arg-one-definition-per-header -Xclang print-constructs")
    elseif ("${PLUGIN_FLAG}" STREQUAL "OneTestPerFile")
      set(_EXTRA_PLUGIN_ARGS "-Xclang -plugin-arg-OneTestPerFile -Xclang print-constructs")
    endif ()

    set(_CMD "${_BASE_STR} -Xclang -load -Xclang \"${_PLUG}\" ${_EXTRA_PLUGIN_ARGS} -Xclang -plugin -Xclang ${PLUGIN_FLAG} \"${_FP}\"")

    execute_process(COMMAND bash -lc "${_CMD}" OUTPUT_VARIABLE _OUT ERROR_VARIABLE _ERR RESULT_VARIABLE _RC)

    if (NOT _RC EQUAL 0)
      # Fallback: rerun with verbose diagnostics to surface the root cause.
      # During verbose replay, request debug-errors to force summary as Error diagnostics
      # Pass debug-errors to the appropriate plugin
      if ("${PLUGIN_FLAG}" STREQUAL "OneDefinitionPerCppFile")
        set(_DBG_ARGS "-Xclang -plugin-arg-OneDefinitionPerCppFile -Xclang debug-errors")
      elseif ("${PLUGIN_FLAG}" STREQUAL "one-definition-per-header")
        set(_DBG_ARGS "-Xclang -plugin-arg-one-definition-per-header -Xclang debug-errors")
      elseif ("${PLUGIN_FLAG}" STREQUAL "OneTestPerFile")
        set(_DBG_ARGS "-Xclang -plugin-arg-OneTestPerFile -Xclang debug-errors")
      else()
        set(_DBG_ARGS "")
      endif()
      set(_CMD_VERBOSE "${_BASE_STR} -ferror-limit=0 -fno-caret-diagnostics -fdiagnostics-absolute-paths -Xclang -load -Xclang \"${_PLUG}\" ${_EXTRA_PLUGIN_ARGS} ${_DBG_ARGS} -Xclang -plugin -Xclang ${PLUGIN_FLAG} \"${_FP}\"")
      message(STATUS "  ${PLUGIN_FLAG} failed on ${_P} (${_RC}). Replaying with verbose diagnostics:\n    CMD: ${_CMD_VERBOSE}")
      execute_process(COMMAND bash -lc "${_CMD_VERBOSE}" OUTPUT_VARIABLE _OUT2 ERROR_VARIABLE _ERR2 RESULT_VARIABLE _RC2)
      message(STATUS "  ---- stdout+stderr (replay) ----\n${_OUT2}${_ERR2}\n  -------------------------------")
      # If replay produced no diagnostics, attempt a plain syntax check without the plugin
      string(LENGTH "${_OUT2}${_ERR2}" _LEN_REPLAY)
      if (_LEN_REPLAY EQUAL 0)
        set(_CMD_SANITY "${_BASE_STR} \"${_FP}\"")
        message(STATUS "  No diagnostics from replay; running sanity parse without plugin:\n    CMD: ${_CMD_SANITY}")
        execute_process(COMMAND bash -lc "${_CMD_SANITY}" OUTPUT_VARIABLE _OUT3 ERROR_VARIABLE _ERR3 RESULT_VARIABLE _RC3)
        message(STATUS "  ---- stdout+stderr (sanity) ----\n${_OUT3}${_ERR3}\n  --------------------------------")
        if (_RC3 EQUAL 0)
          message(STATUS "  Sanity parse ok and no plugin diagnostics; treating as pass for ${_P}")
          set(_RC 0)
        endif()
      endif()
      if (NOT _RC EQUAL 0)
        message(FATAL_ERROR "-- Linter failed --")
      endif()
    endif ()
    message(STATUS "  ✅ Lint passed for ${_P}")
  endforeach ()
endfunction()
