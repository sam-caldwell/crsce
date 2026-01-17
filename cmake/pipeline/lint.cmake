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

# Extend PATH to include local tool dirs (venv and node bins) and brew LLVM
if(DEFINED ENV{PATH})
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin:$ENV{PATH}")
else()
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin")
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
  # Choose from (in order): LINT_BUILD_DIR, build/llvm-debug, build/arm64-debug, first build/* with compile_commands.json
  if(DEFINED LINT_BUILD_DIR)
    set(_BIN_DIR "${LINT_BUILD_DIR}")
  else()
    set(_BIN_DIR "${CMAKE_SOURCE_DIR}/build/llvm-debug")
  endif()
  if(NOT EXISTS "${_BIN_DIR}/compile_commands.json")
    set(_ALT_DIR "${CMAKE_SOURCE_DIR}/build/arm64-debug")
    if(EXISTS "${_ALT_DIR}/compile_commands.json")
      set(_BIN_DIR "${_ALT_DIR}")
    else()
      file(GLOB CC_JSON_FILES "${CMAKE_SOURCE_DIR}/build/*/compile_commands.json")
      list(LENGTH CC_JSON_FILES _CC_LEN)
      if(_CC_LEN GREATER 0)
        list(GET CC_JSON_FILES 0 _CC_PATH)
        get_filename_component(_BIN_DIR "${_CC_PATH}" DIRECTORY)
      else()
        message(FATAL_ERROR "clang-tidy compile database missing. Run 'make configure' (e.g., PRESET=llvm-debug) first.")
      endif()
    endif()
  endif()
  include("${CMAKE_SOURCE_DIR}/cmake/pipeline/sdk.cmake")
  crsce_sdk_env_prefix(_ENV_PREFIX)
  if(_ENV_PREFIX)
    set(_ENV_PREFIX "${_ENV_PREFIX} ")
  endif()
  crsce_sdk_tidy_extra_args(_EXTRA_ARGS_LIST)
  # Use the standard library discovered by the compile database + SDKROOT.
  # Avoid forcing a specific libc++ include path to prevent mismatches with
  # the resource dir that clang-tidy selects.
  set(_EXTRA_ARGS "")
  if(_EXTRA_ARGS_LIST)
    set(_EXTRA_ARGS ${_EXTRA_ARGS_LIST})
  endif()
  # Ensure standard and disable modules during analysis
  # Note: rely on the compile_commands.json toolchain/arch; passing an explicit
  # -target here was causing clang to reject the flag under some driver modes.
  # Let clang infer driver mode from the compile_commands entry.
  # Keep args minimal; rely on compile DB for language/std and toolchain.
  # Ensure resource dir points to Homebrew LLVM (some installs need this)
  set(_RES_DIR "")
  execute_process(COMMAND /opt/homebrew/opt/llvm/bin/clang++ -print-resource-dir OUTPUT_VARIABLE _RES_DIR OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _rd_rc)
  if(_rd_rc EQUAL 0 AND _RES_DIR)
    list(APPEND _EXTRA_ARGS "-extra-arg=-resource-dir=${_RES_DIR}")
  endif()
  set(_EXTRA_STR "")
  # Run clang-tidy over tracked C++ sources using compile_commands.json
  # Prefer run-clang-tidy if available to use compile DB entries per file
  # Avoid known issues with run-clang-tidy + SDK/libc++ mismatches on macOS by
  # invoking clang-tidy directly with explicit include args. This keeps results
  # deterministic across environments.
  set(_HDR_FILTER "^(include|src|cmd|test)/")
  set(_BASE_CMD ${CLANG_TIDY_EXE} -quiet -header-filter=${_HDR_FILTER})
  if(APPLE AND CRSCE_MACOS_SDK)
    list(APPEND _BASE_CMD -extra-arg=-isysroot${CRSCE_MACOS_SDK})
    list(APPEND _BASE_CMD -extra-arg=-stdlib=libc++)
  endif()
  list(APPEND _BASE_CMD -extra-arg=-std=c++23)
  list(APPEND _BASE_CMD -extra-arg=-I${CMAKE_SOURCE_DIR}/include)
  list(APPEND _BASE_CMD -extra-arg=-I${CMAKE_SOURCE_DIR}/src)
  list(APPEND _BASE_CMD -extra-arg=-I${CMAKE_SOURCE_DIR}/src/common)
  list(APPEND _BASE_CMD -extra-arg=-I${CMAKE_SOURCE_DIR}/src/Compress)
  list(APPEND _BASE_CMD -extra-arg=-I${CMAKE_SOURCE_DIR}/src/Decompress)

  # Expand file list via git to mirror tracked sources
  execute_process(
    COMMAND bash -lc "git ls-files '*.cpp'"
    OUTPUT_VARIABLE _SRC_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _ls_rc)
  if(NOT _ls_rc EQUAL 0)
    message(FATAL_ERROR "Failed to enumerate C++ sources for clang-tidy.")
  endif()
  separate_arguments(_SRC_LIST_NL UNIX_COMMAND "${_SRC_LIST}")
  if(NOT _SRC_LIST_NL)
    message(STATUS "No C++ sources found for clang-tidy.")
  else()
    list(LENGTH _SRC_LIST_NL _N)
    math(EXPR _I "0")
    while(_I LESS _N)
      list(GET _SRC_LIST_NL ${_I} _FILE)
      execute_process(COMMAND ${_BASE_CMD} ${_FILE} RESULT_VARIABLE _one_rc)
      if(NOT _one_rc EQUAL 0)
        message(FATAL_ERROR "clang-tidy failed on ${_FILE} (${_one_rc}).")
      endif()
      math(EXPR _I "${_I}+1")
    endwhile()
  endif()
endif()

message(STATUS "✅ Lint complete")
