#[[
  File: cmake/pipeline/lint.cmake
  Brief: Run project linters via CMake script for consistent CI/CLI usage.

  Usage:
    cmake -D LINT_TARGET=all -P cmake/pipeline/lint.cmake
    cmake -D LINT_TARGET=md|sh|py|cpp|make -P cmake/pipeline/lint.cmake
]]

if(NOT CMAKE_SCRIPT_MODE_FILE)
  # Only execute when run as a script via `cmake -P`. If included from
  # CMakeLists, do nothing.
  return()
endif()

if(NOT DEFINED LINT_TARGET)
  set(LINT_TARGET all)
endif()

# Extend PATH to include local tool dirs (venv and node bins)
if(DEFINED ENV{PATH})
  set(ENV{PATH} "${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin:$ENV{PATH}")
else()
  set(ENV{PATH} "${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin")
endif()

function(_run name)
  message(STATUS "Lint: ${name}")
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Lint '${name}' failed (${rc}).")
  endif()
endfunction()

# Markdown
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "md")
  find_program(MARKDOWNLINT_EXE markdownlint)
  if(NOT MARKDOWNLINT_EXE)
    message(FATAL_ERROR "markdownlint not found. Run 'make ready/fix'.")
  endif()
  _run(markdown "${MARKDOWNLINT_EXE}" "**/*.md")
endif()

# Shell
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "sh")
  find_program(SHELLCHECK_EXE shellcheck)
  if(NOT SHELLCHECK_EXE)
    message(FATAL_ERROR "shellcheck not found. Run 'make ready/fix'.")
  endif()
  # Use bash -lc to expand globs/pipe reliably
  _run(shell "bash" "-lc" "find . -name '*.sh' -print0 | xargs -0 -r ${SHELLCHECK_EXE}")
endif()

# Python
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "py")
  find_program(FLAKE8_EXE flake8)
  if(NOT FLAKE8_EXE)
    message(FATAL_ERROR "flake8 not found. Run 'make ready/fix'.")
  endif()
  _run(flake8 "${FLAKE8_EXE}" "--exclude=venv/,./node")
endif()

# Makefiles
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "make")
  find_program(CHECKMAKE_EXE checkmake)
  if(NOT CHECKMAKE_EXE)
    message(FATAL_ERROR "checkmake not found. Run 'make ready/fix'.")
  endif()
  # Expand glob for checkmake (it does not expand shell globs itself)
  file(GLOB MAKE_MK_FILES "${CMAKE_SOURCE_DIR}/Makefile.d/*.mk")
  if(MAKE_MK_FILES)
    _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" "Makefile" ${MAKE_MK_FILES})
  else()
    _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" "Makefile")
  endif()
endif()

# C++ (clang-tidy required)
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "cpp")
  find_program(CLANG_TIDY_EXE clang-tidy)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "clang-tidy is required. Run 'make ready/fix' to install llvm.")
  endif()
  # Ensure we have a configured build for compile_commands
  set(_BIN_DIR "${CMAKE_SOURCE_DIR}/build/arm64-debug")
  if(NOT EXISTS "${_BIN_DIR}/compile_commands.json")
    message(FATAL_ERROR "clang-tidy compile database missing. Run 'make configure' first.")
  endif()
  # Run the CMake-provided clang-tidy target if available, otherwise direct
  execute_process(COMMAND "${CMAKE_COMMAND}" --build "${_BIN_DIR}" --target clang-tidy RESULT_VARIABLE _ct_rc)
  if(NOT _ct_rc EQUAL 0)
    # Direct invocation fallback across tracked sources
    execute_process(
      COMMAND bash -lc "git ls-files '*.cpp' | xargs -n 16 ${CLANG_TIDY_EXE} -p ${_BIN_DIR}"
      RESULT_VARIABLE _dir_rc)
    if(NOT _dir_rc EQUAL 0)
      message(FATAL_ERROR "clang-tidy failed (${_dir_rc}).")
    endif()
  endif()
endif()

message(STATUS "✅ Lint complete")
