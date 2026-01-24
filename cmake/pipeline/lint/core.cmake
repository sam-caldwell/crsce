#[[
  File: cmake/pipeline/lint/core.cmake
  Brief: Core helpers and defaults for the lint pipeline.
]]

include_guard(GLOBAL)

# Only execute when run as a script via `cmake -P` or included from lint.cmake

if (NOT DEFINED LINT_TARGET)
  set(LINT_TARGET all)
endif ()

# By default, lint only files that have changed (opt-out with LINT_CHANGED_ONLY=OFF)
if (NOT DEFINED LINT_CHANGED_ONLY)
  set(LINT_CHANGED_ONLY ON)
endif ()

# Enable parallel helpers for supported linters
if (NOT DEFINED LINT_PARALLEL)
  set(LINT_PARALLEL ON)
endif ()

# Extend PATH to include Homebrew LLVM, venv, and node bins
if (DEFINED ENV{PATH})
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin:$ENV{PATH}")
else ()
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin")
endif ()

# Determine parallelism
include("${CMAKE_SOURCE_DIR}/cmake/tools/num_cpus.cmake")
crsce_num_cpus(_NPROC)
if (NOT _NPROC)
  set(_NPROC 1)
endif ()

function(_run name)
  message(STATUS "Lint: ${name}")
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
  if (NOT rc EQUAL 0)
    message(STATUS "  🔥 Lint '${name}' failed (${rc}).")
    message(FATAL_ERROR "-- 🔥 Linter failed 🔥 --")
  endif ()
  message(STATUS "  ✅ Lint '${name}' ok (${rc}). ✅")
endfunction()

# Helper: gather changed, tracked files matching pathspecs
function(_git_changed_files out)
  if (NOT LINT_CHANGED_ONLY)
    set(${out} "" PARENT_SCOPE)
    return()
  endif ()
  string(REPLACE ";" " " _GIT_PATHSPEC "${ARGN}")
  set(_cmd "set -euo pipefail;\n\
  upstream=\"\$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null || true)\";\n\
  {\n\
    git diff --name-only --diff-filter=ACMR --cached -- ${_GIT_PATHSPEC} || true;\n\
    git diff --name-only --diff-filter=ACMR -- ${_GIT_PATHSPEC} || true;\n\
    if [ -n \"$upstream\" ]; then\n\
      git diff --name-only --diff-filter=ACMR \"$upstream\"...HEAD -- ${_GIT_PATHSPEC} || true;\n\
    fi;\n\
  } | sort -u | awk 'NF' | while IFS= read -r f; do [ -e \"$f\" ] && printf '%s\n' \"$f\"; done")
  execute_process(
          COMMAND bash -lc "${_cmd}"
          WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
          OUTPUT_VARIABLE _files
          OUTPUT_STRIP_TRAILING_WHITESPACE
          RESULT_VARIABLE _rc)
  if (NOT _rc EQUAL 0)
    set(_files "")
  endif ()
  if (_files)
    string(REPLACE "\n" ";" _list "${_files}")
    set(${out} ${_list} PARENT_SCOPE)
  else ()
    set(${out} "" PARENT_SCOPE)
  endif ()
endfunction()
