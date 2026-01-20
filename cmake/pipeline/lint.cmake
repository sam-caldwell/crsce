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

# By default, lint only files that have changed.
# This scopes linting during local development and CI diffs.
# Set LINT_CHANGED_ONLY=OFF to lint everything.
if(NOT DEFINED LINT_CHANGED_ONLY)
  set(LINT_CHANGED_ONLY ON)
endif()

# Enable parallel clang-tidy via xargs by default for speed
if(NOT DEFINED LINT_PARALLEL)
  set(LINT_PARALLEL ON)
endif()

# Extend PATH to include local tool dirs (venv and node bins) and brew LLVM
if(DEFINED ENV{PATH})
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin:$ENV{PATH}")
else()
  set(ENV{PATH} "/opt/homebrew/opt/llvm/bin:${CMAKE_SOURCE_DIR}/venv/bin:${CMAKE_SOURCE_DIR}/node/node_modules/.bin")
endif()

# Determine reasonable parallelism for tools that support it
include("${CMAKE_SOURCE_DIR}/cmake/tools/num_cpus.cmake")
crsce_num_cpus(_NPROC)
if(NOT _NPROC)
  set(_NPROC 1)
endif()

function(_run name)
  message(STATUS "Lint: ${name}")
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(STATUS "-- Linter failed --")
    message(FATAL_ERROR "Lint '${name}' failed (${rc}).")
  endif()
endfunction()

# Helper: gather changed files (tracked) matching a git pathspec list.
# - Includes unstaged, staged, and (if available) diff vs upstream.
# - Output variable receives a CMake list of files (relative paths).
function(_git_changed_files out)
  if(NOT LINT_CHANGED_ONLY)
    set(${out} "" PARENT_SCOPE)
    return()
  endif()
  # Join pathspecs for shell command
  string(REPLACE ";" " " _GIT_PATHSPEC "${ARGN}")
  set(_cmd "set -euo pipefail;\n\
    upstream=\"\$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null || true)\";\n\
    {\n\
      git diff --name-only --diff-filter=ACMR --cached -- ${_GIT_PATHSPEC} || true;\n\
      git diff --name-only --diff-filter=ACMR -- ${_GIT_PATHSPEC} || true;\n\
      if [ -n \"$upstream\" ]; then\n\
        git diff --name-only --diff-filter=ACMR \"$upstream\"...HEAD -- ${_GIT_PATHSPEC} || true;\n\
      fi;\n\
    } | sort -u | awk 'NF' ")
  execute_process(
    COMMAND bash -lc "${_cmd}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE _files
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _rc)
  if(NOT _rc EQUAL 0)
    # If git isn't available or not a repo, return empty (no changed files)
    set(_files "")
  endif()
  if(_files)
    string(REPLACE "\n" ";" _list "${_files}")
    set(${out} ${_list} PARENT_SCOPE)
  else()
    set(${out} "" PARENT_SCOPE)
  endif()
endfunction()

# Markdown
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "md")
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_MD_CHANGED "*.md")
    if(NOT _MD_CHANGED)
      message(STATUS "Markdown: no changed files; skipping")
    else()
      find_program(MARKDOWNLINT_EXE markdownlint)
      if(NOT MARKDOWNLINT_EXE)
        message(FATAL_ERROR "markdownlint not found. Run 'make ready/fix'.")
      endif()
      _run(markdown "${MARKDOWNLINT_EXE}" "--ignore" "build/**" ${_MD_CHANGED})
    endif()
  else()
    find_program(MARKDOWNLINT_EXE markdownlint)
    if(NOT MARKDOWNLINT_EXE)
      message(FATAL_ERROR "markdownlint not found. Run 'make ready/fix'.")
    endif()
    # Exclude generated content under build/*
    _run(markdown "${MARKDOWNLINT_EXE}" "--ignore" "build/**" "**/*.md")
  endif()
endif()

# Shell
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "sh")
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_SH_CHANGED "*.sh")
    if(NOT _SH_CHANGED)
      message(STATUS "Shell: no changed files; skipping")
    else()
      find_program(SHELLCHECK_EXE shellcheck)
      if(NOT SHELLCHECK_EXE)
        message(FATAL_ERROR "shellcheck not found. Run 'make ready/fix'.")
      endif()
      foreach(_sh ${_SH_CHANGED})
        _run(shell "${SHELLCHECK_EXE}" "${_sh}")
      endforeach()
    endif()
  else()
    find_program(SHELLCHECK_EXE shellcheck)
    if(NOT SHELLCHECK_EXE)
      message(FATAL_ERROR "shellcheck not found. Run 'make ready/fix'.")
    endif()
    # Use bash -lc to expand globs/pipe reliably
    # Prune build/* from search to avoid generated files
    _run(shell "bash" "-lc" "find . -path './build' -prune -o -name '*.sh' -print0 | xargs -0 -r -P ${_NPROC} -n 1 ${SHELLCHECK_EXE}")
  endif()
endif()

# Python
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "py")
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_PY_CHANGED "*.py")
    if(NOT _PY_CHANGED)
      message(STATUS "Python: no changed files; skipping")
    else()
      find_program(FLAKE8_EXE flake8)
      if(NOT FLAKE8_EXE)
        message(FATAL_ERROR "flake8 not found. Run 'make ready/fix'.")
      endif()
      _run(flake8 "${FLAKE8_EXE}" "--jobs=${_NPROC}" ${_PY_CHANGED})
    endif()
  else()
    find_program(FLAKE8_EXE flake8)
    if(NOT FLAKE8_EXE)
      message(FATAL_ERROR "flake8 not found. Run 'make ready/fix'.")
    endif()
    # Exclude venv, node packages, and build outputs
    _run(flake8 "${FLAKE8_EXE}" "--exclude=venv/,./node,./build" "--jobs=${_NPROC}")
  endif()
endif()

# Makefiles
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "make")
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_MK_CHANGED "Makefile" "Makefile.d/*.mk")
    if(NOT _MK_CHANGED)
      message(STATUS "Makefiles: no changed files; skipping")
    else()
      find_program(CHECKMAKE_EXE checkmake)
      if(NOT CHECKMAKE_EXE)
        message(FATAL_ERROR "checkmake not found. Run 'make ready/fix'.")
      endif()
      _run(checkmake "${CHECKMAKE_EXE}" "--config=.checkmake.conf" ${_MK_CHANGED})
    endif()
  else()
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
endif()

# C++ (clang-tidy required)
if((LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "cpp") AND NOT LINT_ONLY_TOOLS)
  set(_DO_CPP TRUE)
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_CPP_CHANGED "*.cpp")
    if(NOT _CPP_CHANGED)
      message(STATUS "C++: no changed sources; skipping")
      set(_DO_CPP FALSE)
    endif()
  endif()
  if(NOT _DO_CPP)
    # Skip the remainder of the C++ lint block
  else()
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
    # Hard guard: if no compilation database is available, fail with guidance.
    if(NOT EXISTS "${_BIN_DIR}/compile_commands.json")
      message(FATAL_ERROR
        "clang-tidy compile database missing at '${_BIN_DIR}'.\n"
        "Run 'make configure PRESET=llvm-debug' (or your preset) before 'make lint'.")
    endif()
    # Run clang-tidy over tracked C++ sources using compile_commands.json
    # Prefer run-clang-tidy if available to use compile DB entries per file
    # Avoid known issues with run-clang-tidy + SDK/libc++ mismatches on macOS by
    # invoking clang-tidy directly with explicit include args. This keeps results
    # deterministic across environments.
    # Restrict header analysis to project sources for speed
    set(_HDR_FILTER "^(include|src|cmd|test)/")
    # Show full diagnostics from clang-tidy and include which -W option triggered them.
    # Suppress the compiler's own warnings (and its noisy "warnings generated" summary)
    # so we only see actionable clang-tidy diagnostics.
    set(_BASE_CMD ${CLANG_TIDY_EXE} -p ${_BIN_DIR} -warnings-as-errors=* "-header-filter=${_HDR_FILTER}")
    # Load any clang-tidy plugins built under build/tools/clang-tidy-plugins/**
    set(_TIDY_PLUGIN_LIBS)
    file(GLOB_RECURSE _PLUG_DYLIBS_1 "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.dylib")
    file(GLOB_RECURSE _PLUG_SOS_1    "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.so")
    file(GLOB_RECURSE _PLUG_DLLS_1   "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.dll")
    file(GLOB_RECURSE _PLUG_DYLIBS_2 "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.dylib")
    file(GLOB_RECURSE _PLUG_SOS_2    "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.so")
    file(GLOB_RECURSE _PLUG_DLLS_2   "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.dll")
    set(_TIDY_PLUGIN_LIBS ${_PLUG_DYLIBS_1} ${_PLUG_SOS_1} ${_PLUG_DLLS_1} ${_PLUG_DYLIBS_2} ${_PLUG_SOS_2} ${_PLUG_DLLS_2})
    list(LENGTH _TIDY_PLUGIN_LIBS _TPL_N)
    if(_TPL_N GREATER 0)
      foreach(_plib IN LISTS _TIDY_PLUGIN_LIBS)
        get_filename_component(_plib_dir "${_plib}" DIRECTORY)
        if(EXISTS "${_plib_dir}/.is_clang_tidy_plugin")
          list(APPEND _BASE_CMD -load "${_plib}")
        endif()
      endforeach()
    endif()
    list(APPEND _BASE_CMD -extra-arg=-w)
    list(APPEND _BASE_CMD -extra-arg=-fdiagnostics-show-option)
    list(APPEND _BASE_CMD -extra-arg=-fcolor-diagnostics)
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
    # Ensure GoogleTest headers are found even before tests build
    list(APPEND _BASE_CMD -extra-arg=-I${_BIN_DIR}/_deps/googletest-src/googletest/include)
    list(APPEND _BASE_CMD -extra-arg=-I${_BIN_DIR}/_deps/googletest-src/googletest)
    # Avoid redefining builtin macros; do not override __has_feature.

  # Determine which C++ sources to lint
    if(LINT_CHANGED_ONLY)
      set(_SRC_LIST_NL ${_CPP_CHANGED})
      # Filter out files that no longer exist (deleted/renamed in this branch)
      if(_SRC_LIST_NL)
        set(_EXISTING)
        foreach(_FILE IN LISTS _SRC_LIST_NL)
          if(IS_ABSOLUTE "${_FILE}")
            set(_CHK "${_FILE}")
          else()
            set(_CHK "${CMAKE_SOURCE_DIR}/${_FILE}")
          endif()
          if(EXISTS "${_CHK}")
            list(APPEND _EXISTING "${_FILE}")
          endif()
        endforeach()
        set(_SRC_LIST_NL ${_EXISTING})
      endif()
    else()
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
  # Filter out files that no longer exist
  if(_SRC_LIST_NL)
    set(_EXISTING)
    foreach(_FILE IN LISTS _SRC_LIST_NL)
      if(IS_ABSOLUTE "${_FILE}")
        set(_CHK "${_FILE}")
      else()
        set(_CHK "${CMAKE_SOURCE_DIR}/${_FILE}")
      endif()
      if(EXISTS "${_CHK}")
        list(APPEND _EXISTING "${_FILE}")
      endif()
    endforeach()
    set(_SRC_LIST_NL ${_EXISTING})
  endif()
  # Exclude plugin test fixtures that are intentionally non-compilable (relative paths)
  if(_SRC_LIST_NL)
    list(FILTER _SRC_LIST_NL EXCLUDE REGEX "^test/tools/OneTestPerFile/fixtures/.*\\.cpp$")
    list(FILTER _SRC_LIST_NL EXCLUDE REGEX "^test/tools/clang-plugins/OneTestPerFile/fixtures/.*\\.cpp$")
    list(FILTER _SRC_LIST_NL EXCLUDE REGEX "^test/tools/clang-plugins/.*/fixtures/.*\\.(cpp|h)$")
  endif()
    endif()
    # Exclude plugin test fixtures as well if entries are absolute
    if(_SRC_LIST_NL)
      list(FILTER _SRC_LIST_NL EXCLUDE REGEX ".*/test/tools/OneTestPerFile/fixtures/.*\\.cpp$")
      list(FILTER _SRC_LIST_NL EXCLUDE REGEX ".*/test/tools/clang-plugins/OneTestPerFile/fixtures/.*\\.cpp$")
      list(FILTER _SRC_LIST_NL EXCLUDE REGEX ".*/test/tools/clang-plugins/.*/fixtures/.*\\.(cpp|h)$")
    endif()
    if(NOT _SRC_LIST_NL)
      message(STATUS "No C++ sources found for clang-tidy.")
    else()
    if(LINT_PARALLEL)
      # Build absolute file list for xargs
      set(_ABS_LIST)
      foreach(_FILE IN LISTS _SRC_LIST_NL)
        if(IS_ABSOLUTE "${_FILE}")
          list(APPEND _ABS_LIST "${_FILE}")
        else()
          get_filename_component(_ABS_FILE "${CMAKE_SOURCE_DIR}/${_FILE}" ABSOLUTE)
          list(APPEND _ABS_LIST "${_ABS_FILE}")
        endif()
      endforeach()
      if(_ABS_LIST)
        list(JOIN _ABS_LIST "\n" _ABS_JOINED)
        set(_LIST_FILE "${_BIN_DIR}/.clang_tidy_files.txt")
        file(WRITE "${_LIST_FILE}" "${_ABS_JOINED}\n")
        list(JOIN _BASE_CMD " " _BASE_STR)
        # Quote header-filter regex to avoid shell parsing of () and |
        if(DEFINED _HDR_FILTER)
          string(REPLACE "-header-filter=${_HDR_FILTER}" "'-header-filter=${_HDR_FILTER}'" _BASE_STR "${_BASE_STR}")
        endif()
        # Use -isysroot (already in _BASE_STR when APPLE) rather than SDKROOT= prefix for xargs safety
        set(_XARGS_CMD "xargs -P ${_NPROC} -n 1 -I {} ${_BASE_STR} {} < \"${_LIST_FILE}\"")
        execute_process(COMMAND bash -lc "${_XARGS_CMD}" RESULT_VARIABLE _xrc)
        if(NOT _xrc EQUAL 0)
          message(STATUS "-- Linter failed --")
          message(FATAL_ERROR "clang-tidy failed (parallel mode).")
        endif()
      endif()
    else()
      list(LENGTH _SRC_LIST_NL _N)
      math(EXPR _I "0")
      while(_I LESS _N)
        list(GET _SRC_LIST_NL ${_I} _FILE)
        # Use absolute paths so clang-tidy matches compile_commands entries.
        if(IS_ABSOLUTE "${_FILE}")
          set(_ABS_FILE "${_FILE}")
        else()
          get_filename_component(_ABS_FILE "${CMAKE_SOURCE_DIR}/${_FILE}" ABSOLUTE)
        endif()
        if(CRSCE_MACOS_SDK)
          execute_process(COMMAND ${CMAKE_COMMAND} -E env SDKROOT=${CRSCE_MACOS_SDK} ${_BASE_CMD} ${_ABS_FILE} RESULT_VARIABLE _one_rc)
        else()
          execute_process(COMMAND ${_BASE_CMD} ${_ABS_FILE} RESULT_VARIABLE _one_rc)
        endif()
        if(NOT _one_rc EQUAL 0)
          message(STATUS "-- Linter failed --")
          message(FATAL_ERROR "clang-tidy failed on ${_FILE} (${_one_rc}).")
        endif()
        math(EXPR _I "${_I}+1")
      endwhile()
    endif()
    endif()
  endif()
endif()

# C++ (tools/**) — lint changed tool sources with appropriate compile DBs
if(LINT_TARGET STREQUAL "all" OR LINT_TARGET STREQUAL "cpp" OR LINT_TARGET STREQUAL "cpp_tools")
  find_program(CLANG_TIDY_EXE clang-tidy)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR "clang-tidy is required. Run 'make ready/fix' to install llvm.")
  endif()

  # Identify changed tool sources
  set(_TOOLS_FILES)
  if(LINT_CHANGED_ONLY)
    _git_changed_files(_TOOLS_CHANGED "tools/*.cpp" "tools/*/*.cpp" "tools/*/*/*.cpp" "tools/*/*/*/*.cpp")
    foreach(_f ${_TOOLS_CHANGED})
      if(_f MATCHES "^tools/.*\\.cpp$")
        list(APPEND _TOOLS_FILES "${_f}")
      endif()
    endforeach()
    if(NOT _TOOLS_FILES)
      message(STATUS "C++ (tools): no changed sources; skipping")
    endif()
  else()
    execute_process(
      COMMAND bash -lc "git ls-files 'tools/*.cpp' 'tools/*/*.cpp' 'tools/*/*/*.cpp' 'tools/*/*/*/*.cpp'"
      OUTPUT_VARIABLE _ls
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _lsrc)
    if(_lsrc EQUAL 0 AND _ls)
      separate_arguments(_TOOLS_FILES UNIX_COMMAND "${_ls}")
    endif()
  endif()

  if(_TOOLS_FILES)
    # SDK/Resource-dir helpers
    include("${CMAKE_SOURCE_DIR}/cmake/pipeline/sdk.cmake")
    crsce_sdk_env_prefix(_ENV_PREFIX)
    if(_ENV_PREFIX)
      set(_ENV_PREFIX "${_ENV_PREFIX} ")
    endif()
    set(_EXTRA_ARGS)
    if(APPLE AND CRSCE_MACOS_SDK)
      list(APPEND _EXTRA_ARGS "-extra-arg=-isysroot${CRSCE_MACOS_SDK}")
      list(APPEND _EXTRA_ARGS "-extra-arg=-stdlib=libc++")
    endif()
    # Try llvm-config for LLVM/Clang include dir; fallback to Homebrew path
    set(_LLVM_INC_DIR "")
    find_program(LLVM_CONFIG_EXE llvm-config)
    if(LLVM_CONFIG_EXE)
      execute_process(COMMAND ${LLVM_CONFIG_EXE} --includedir OUTPUT_VARIABLE _LLVM_INC_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    if(NOT _LLVM_INC_DIR)
      set(_LLVM_INC_DIR "/opt/homebrew/opt/llvm/include")
    endif()
    if(EXISTS "${_LLVM_INC_DIR}")
      list(APPEND _EXTRA_ARGS "-extra-arg=-I${_LLVM_INC_DIR}")
    endif()
    # Resource dir to find builtin headers
    set(_RES_DIR "")
    execute_process(COMMAND /opt/homebrew/opt/llvm/bin/clang++ -print-resource-dir OUTPUT_VARIABLE _RES_DIR OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _rd_rc)
    if(_rd_rc EQUAL 0 AND _RES_DIR)
      list(APPEND _EXTRA_ARGS "-extra-arg=-resource-dir=${_RES_DIR}")
    endif()

    # Compute group for each file and choose its compile DB under build/tools/<group>
    foreach(_f ${_TOOLS_FILES})
      # Compute group relative to tools/
      get_filename_component(_dir "${_f}" DIRECTORY)
      string(REPLACE "tools/" "" _rel_dir "${_dir}")
      set(_grp "${_rel_dir}")
      if(_grp MATCHES "/src$")
        string(REGEX REPLACE "/src$" "" _grp "${_grp}")
      endif()
      set(_grp_build "${CMAKE_SOURCE_DIR}/build/tools/${_grp}")
      set(_p "${_grp_build}")
      if(NOT EXISTS "${_p}/compile_commands.json")
        # Fallback to main compile DB if group DB missing
        set(_p "${CMAKE_SOURCE_DIR}/build/llvm-debug")
      endif()

      # Compose clang-tidy command
      # Restrict header analysis to project sources and tools for speed
      set(_BASE ${CLANG_TIDY_EXE} -p ${_p} -warnings-as-errors=* "-header-filter=^(tools|include|src|cmd|test)/")
      # Load any clang-tidy plugins built under build/tools/clang-tidy-plugins/**
      set(_TIDY_PLUGIN_LIBS)
      file(GLOB_RECURSE _PLUG_DYLIBS_1 "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.dylib")
      file(GLOB_RECURSE _PLUG_SOS_1    "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.so")
      file(GLOB_RECURSE _PLUG_DLLS_1   "${CMAKE_SOURCE_DIR}/build/tools/clang-tidy-plugins/*/*.dll")
      file(GLOB_RECURSE _PLUG_DYLIBS_2 "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.dylib")
      file(GLOB_RECURSE _PLUG_SOS_2    "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.so")
      file(GLOB_RECURSE _PLUG_DLLS_2   "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/*/*.dll")
      set(_TIDY_PLUGIN_LIBS ${_PLUG_DYLIBS_1} ${_PLUG_SOS_1} ${_PLUG_DLLS_1} ${_PLUG_DYLIBS_2} ${_PLUG_SOS_2} ${_PLUG_DLLS_2})
      list(LENGTH _TIDY_PLUGIN_LIBS _TPL_N)
      if(_TPL_N GREATER 0)
        foreach(_plib IN LISTS _TIDY_PLUGIN_LIBS)
          get_filename_component(_plib_dir "${_plib}" DIRECTORY)
          if(EXISTS "${_plib_dir}/.is_clang_tidy_plugin")
            list(APPEND _BASE -load "${_plib}")
          endif()
        endforeach()
      endif()
      list(APPEND _BASE -extra-arg=-w -extra-arg=-fdiagnostics-show-option -extra-arg=-fcolor-diagnostics)
      list(APPEND _BASE ${_EXTRA_ARGS})
      # Ensure GoogleTest headers are found for tool tests that include gtest
      set(_GTEST_ROOT "${CMAKE_SOURCE_DIR}/build/llvm-debug/_deps/googletest-src/googletest")
      list(APPEND _BASE -extra-arg=-I${_GTEST_ROOT}/include -extra-arg=-I${_GTEST_ROOT})
      # Absolute file path
      get_filename_component(_abs "${CMAKE_SOURCE_DIR}/${_f}" ABSOLUTE)
      execute_process(COMMAND ${_BASE} ${_abs} RESULT_VARIABLE _rc)
      if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "clang-tidy failed on ${_f} (${_rc}).")
      endif()
    endforeach()
  endif()
endif()

message(STATUS "✅ Lint complete")
