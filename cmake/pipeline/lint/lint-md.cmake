#[[
  File: cmake/pipeline/lint/lint-md.cmake
  Brief: Markdown lint using markdownlint.
]]

include_guard(GLOBAL)

function(lint_md)

  find_program(MARKDOWNLINT_EXE markdownlint)
  if(NOT MARKDOWNLINT_EXE)
    message(FATAL_ERROR " 💀 markdownlint not found. Run 'make ready/fix'.")
  endif()

  if(LINT_CHANGED_ONLY)
    _git_changed_files(_MD_CHANGED "*.md")
    if(NOT _MD_CHANGED)
      message(STATUS "Markdown: no changed files; skipping 👍")
      return()
    endif()
    _run(markdown "${MARKDOWNLINT_EXE}" "--ignore" "build/**" ${_MD_CHANGED})
  else()
    _run(markdown "${MARKDOWNLINT_EXE}" "--ignore" "build/**" "**/*.md")
  endif()
endfunction()
