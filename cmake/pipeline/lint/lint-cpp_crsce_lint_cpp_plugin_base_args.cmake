#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_plugin_base_args.cmake
  Brief: Compose base clang++ args for plugin checks.
]]
include_guard(GLOBAL)

function(_crsce_lint_cpp_plugin_base_args BIN_DIR OUT_STR)
  # Detect clang++ executable (prefer Homebrew LLVM on macOS)
  if(EXISTS "/opt/homebrew/opt/llvm/bin/clang++")
    set(_CLANGXX "/opt/homebrew/opt/llvm/bin/clang++")
  else()
    find_program(_CLANGXX clang++)
    if(NOT _CLANGXX)
      set(_CLANGXX clang++)
    endif()
  endif()

  set(_BASE_ARGS "${_CLANGXX} -fsyntax-only -std=c++23 -x c++")
  # Improve diagnostic visibility from plugin runs
  list(APPEND _BASE_ARGS -fcolor-diagnostics -fdiagnostics-absolute-paths -fno-caret-diagnostics -fdiagnostics-show-option)
  list(APPEND _BASE_ARGS -I${CMAKE_SOURCE_DIR}/include -I${CMAKE_SOURCE_DIR}/src -I${CMAKE_SOURCE_DIR}/src/common -I${CMAKE_SOURCE_DIR}/src/Compress -I${CMAKE_SOURCE_DIR}/src/Decompress -I${CMAKE_SOURCE_DIR}/test)
  list(APPEND _BASE_ARGS -I${BIN_DIR}/_deps/googletest-src/googletest/include -I${BIN_DIR}/_deps/googletest-src/googletest)
  # Provide common test compile definitions used across the suite
  # Escape quotes so they survive bash -lc and reach clang as a string literal
  list(APPEND _BASE_ARGS -DTEST_BINARY_DIR=\\\"${BIN_DIR}\\\")
  execute_process(COMMAND ${_CLANGXX} -print-resource-dir OUTPUT_VARIABLE _RD OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE _rc)
  if(_rc EQUAL 0 AND _RD)
    list(APPEND _BASE_ARGS -resource-dir=${_RD})
  endif()
  # On macOS, ensure sysroot is provided so libc++ headers resolve consistently.
  # Prefer detected SDK via xcrun over ENV{SDKROOT}, which may be unset or stale during git hooks.
  include("${CMAKE_SOURCE_DIR}/cmake/pipeline/sdk.cmake")
  if(APPLE AND CRSCE_MACOS_SDK)
    list(APPEND _BASE_ARGS -isysroot${CRSCE_MACOS_SDK})
  endif()
  string(JOIN " " _BASE_STR ${_BASE_ARGS})
  set(${OUT_STR} "${_BASE_STR}" PARENT_SCOPE)
endfunction()
