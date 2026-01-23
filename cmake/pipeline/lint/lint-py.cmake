#[[
  File: cmake/pipeline/lint/lint-py.cmake
  Brief: Python lint using flake8.
]]

include_guard(GLOBAL)

function(lint_py)
  find_program(FLAKE8_EXE flake8)
  if(NOT FLAKE8_EXE)
    message(FATAL_ERROR "💀 flake8 not found. Run 'make ready/fix'.")
  endif()
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_PY_CHANGED "*.py")
    if(NOT _PY_CHANGED)
      message(STATUS "Python: no changed files; skipping 👍")
      return()
    endif()
    _run(flake8 "${FLAKE8_EXE}" "--jobs=${_NPROC}" ${_PY_CHANGED})
  else()
    _run(flake8 "${FLAKE8_EXE}" "--exclude=venv/,./node,./build" "--jobs=${_NPROC}")
  endif()
endfunction()
