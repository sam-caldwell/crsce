#[[
  File: cmake/pipeline/lint/lint-ws.cmake
  Brief: Whitespace lint (git diff --check for staged and unstaged).
]]

include_guard(GLOBAL)

function(lint_ws)
  message(STATUS "Lint: whitespace")
  execute_process(
    COMMAND bash -lc "set -euo pipefail; \
      if git rev-parse --git-dir >/dev/null 2>&1; then \
        o1=\"\$(git diff --check --cached -- . || true)\"; \
        o2=\"\$(git diff --check -- . || true)\"; \
        errs=\"\$(printf '%s\n%s\n' \"$o1\" \"$o2\" | awk 'NF')\"; \
      else \
        errs=\"\"; \
      fi; \
      if [ -n \"$errs\" ]; then echo \"$errs\"; exit 2; fi"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _WS_OUT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _WS_RC
  )
  if(NOT _WS_RC EQUAL 0)
    message(STATUS "--  Linter failed  --")
    message(FATAL_ERROR "  Whitespace issues detected in diffs:\n${_WS_OUT}")
  endif()
endfunction()
