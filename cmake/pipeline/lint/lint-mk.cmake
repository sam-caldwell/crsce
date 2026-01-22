#[[
  File: cmake/pipeline/lint/lint-mk.cmake
  Brief: Makefile lint using checkmake.
]]

include_guard(GLOBAL)

function(lint_mk)
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_MK_CHANGED "Makefile" "Makefile.d/*.mk")
    if(NOT _MK_CHANGED)
      message(STATUS "Makefiles: no changed files; skipping")
      return()
    endif()
    find_program(CHECKMAKE_EXE checkmake)
    if(NOT CHECKMAKE_EXE)
      message(FATAL_ERROR "checkmake not found. Run 'make ready/fix'.")
    endif()
    _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" ${_MK_CHANGED})
  else()
    find_program(CHECKMAKE_EXE checkmake)
    if(NOT CHECKMAKE_EXE)
      message(FATAL_ERROR "checkmake not found. Run 'make ready/fix'.")
    endif()
    file(GLOB MAKE_MK_FILES "${CMAKE_SOURCE_DIR}/Makefile.d/*.mk")
    if(MAKE_MK_FILES)
      _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" "Makefile" ${MAKE_MK_FILES})
    else()
      _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" "Makefile")
    endif()
  endif()
endfunction()
