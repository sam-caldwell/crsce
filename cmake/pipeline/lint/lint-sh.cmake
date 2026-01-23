#[[
  File: cmake/pipeline/lint/lint-sh.cmake
  Brief: Shell script lint using shellcheck.
]]

include_guard(GLOBAL)

function(lint_sh)
  find_program(SHELLCHECK_EXE shellcheck)
  if(NOT SHELLCHECK_EXE)
    message(FATAL_ERROR "💀shellcheck not found. Run 'make ready/fix'.")
  endif()
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_SH_CHANGED "*.sh")
    if(NOT _SH_CHANGED)
      message(STATUS "Shell: no changed files; skipping 👍")
      return()
    endif()
    foreach(_sh ${_SH_CHANGED})
      _run(shell "${SHELLCHECK_EXE}" "${_sh}")
    endforeach()
  else()
    _run(shell "bash" "-lc" "find . -path './build' -prune -o -name '*.sh' -print0 | xargs -0 -r -P ${_NPROC} -n 1 ${SHELLCHECK_EXE}")
  endif()
endfunction()
