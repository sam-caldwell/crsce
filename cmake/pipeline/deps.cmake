#[[
  File: cmake/pipeline/deps.cmake
  Brief: Discover and build tools/**/*.cpp projects. Builds only when sources change.

  Behavior:
    - Groups sources under tools/<group>/ (group is usually the dir above src/).
    - For groups under tools/clang-plugins/*, builds a shared library plugin.
    - For groups under tools/clang-tidy-plugins/*, builds a shared library plugin.
    - For other groups, builds an executable from the group's sources.
    - Skips configure/build if no .cpp files in the group changed since last build.

  Usage:
    cmake -D SOURCE_DIR=<repo_root> -D BUILD_DIR=<build_root> -P cmake/pipeline/deps.cmake
]]

if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${CMAKE_SOURCE_DIR}")
endif()
if(NOT DEFINED BUILD_DIR)
  set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
endif()

include("${SOURCE_DIR}/cmake/pipeline/sdk.cmake")
include("${SOURCE_DIR}/cmake/tools/num_cpus.cmake")

set(TOOLS_DIR "${SOURCE_DIR}/tools")
if(NOT EXISTS "${TOOLS_DIR}")
  message(STATUS "No tools/ directory present; nothing to build.")
  return()
endif()

crsce_num_cpus(_NPROC)
if(NOT _NPROC)
  set(_NPROC 1)
endif()
crsce_sdk_configure_args(_SDK_CFG_FLAG)

# Collect all .cpp sources under tools/
file(GLOB_RECURSE _ALL_CPPS RELATIVE "${TOOLS_DIR}" "${TOOLS_DIR}/*.cpp")
if(NOT _ALL_CPPS)
  message(STATUS "No .cpp sources under tools/. Nothing to build.")
  return()
endif()

# Determine unique groups (strip trailing /src from the directory)
set(_GROUPS)
foreach(_rel_cpp IN LISTS _ALL_CPPS)
  get_filename_component(_src_dir "${_rel_cpp}" DIRECTORY)
  set(_grp "${_src_dir}")
  if(_grp MATCHES "/src$")
    string(REGEX REPLACE "/src$" "" _grp "${_grp}")
  endif()
  list(APPEND _GROUPS "${_grp}")
endforeach()
list(REMOVE_DUPLICATES _GROUPS)

# Optional: build only a specific group (e.g., ONLY_GROUP=clang-plugins/OneDefinitionPerHeader)
if(DEFINED ONLY_GROUP AND NOT "${ONLY_GROUP}" STREQUAL "")
  set(_FOUND FALSE)
  foreach(_g IN LISTS _GROUPS)
    if(_g STREQUAL "${ONLY_GROUP}")
      set(_FOUND TRUE)
      break()
    endif()
  endforeach()
  if(_FOUND)
    set(_GROUPS "${ONLY_GROUP}")
  else()
    message(STATUS "Requested ONLY_GROUP='${ONLY_GROUP}' not found; skipping build.")
    set(_GROUPS)
  endif()
endif()

set(_FAILURES 0)
foreach(_grp IN LISTS _GROUPS)
  if(_grp STREQUAL "")
    continue()
  endif()

  # Determine the sources in this group
  set(_GRP_CPPS)
  foreach(_rel_cpp IN LISTS _ALL_CPPS)
    if(_rel_cpp MATCHES "^${_grp}(/|$)")
      list(APPEND _GRP_CPPS "${TOOLS_DIR}/${_rel_cpp}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _GRP_CPPS)
  if(NOT _GRP_CPPS)
    continue()
  endif()

  # Build directory for this group
  set(_BUILD_DIR_GRP "${BUILD_DIR}/tools/${_grp}")
  # clang-tidy plugins are built under build/tools/clang-plugins/<PluginName>
  if(_grp MATCHES "^clang-tidy-plugins/")
    get_filename_component(_GRP_BASENAME "${_grp}" NAME)
    set(_BUILD_DIR_GRP "${BUILD_DIR}/tools/clang-plugins/${_GRP_BASENAME}")
  endif()
  file(MAKE_DIRECTORY "${_BUILD_DIR_GRP}")

  # Compute latest .cpp timestamp in this group
  set(_LATEST 0)
  foreach(_src IN LISTS _GRP_CPPS)
    file(TIMESTAMP "${_src}" _ts UTC "%s")
    if(_ts GREATER _LATEST)
      set(_LATEST "${_ts}")
    endif()
  endforeach()

  set(_STAMP_FILE "${_BUILD_DIR_GRP}/.cpp.latest")
  set(_PREV 0)
  if(EXISTS "${_STAMP_FILE}")
    file(READ "${_STAMP_FILE}" _prev_str)
    string(REGEX MATCH "^[0-9]+" _PREV "${_prev_str}")
    if(NOT _PREV)
      set(_PREV 0)
    endif()
  endif()

  if(_LATEST LESS_EQUAL _PREV)
    message(STATUS "Up-to-date; skipping: ${_grp}")
    continue()
  endif()

  # Generate a minimal CMake project in the build dir and compile
  get_filename_component(_GRP_NAME "${_grp}" NAME)
  string(REPLACE "-" "_" _TGT "${_GRP_NAME}")

  set(_CM "cmake_minimum_required(VERSION 3.16)\n")
  string(APPEND _CM "project(${_TGT} LANGUAGES C CXX)\n")
  string(APPEND _CM "set(CMAKE_CXX_STANDARD 17)\n")
  string(APPEND _CM "set(CMAKE_POSITION_INDEPENDENT_CODE ON)\n")
  string(APPEND _CM "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n")
  string(APPEND _CM "if(APPLE)\n  set(CMAKE_MACOSX_RPATH ON)\nendif()\n")

  # Join sources into CMake list
  set(_SRC_LINES)
  foreach(_s IN LISTS _GRP_CPPS)
    string(APPEND _SRC_LINES "  \"${_s}\"\n")
  endforeach()

  if(_grp MATCHES "^(clang-plugins|clang-tidy-plugins)/")
    string(APPEND _CM "find_package(LLVM REQUIRED CONFIG)\nfind_package(Clang REQUIRED CONFIG)\n")
    string(APPEND _CM "include_directories(\${LLVM_INCLUDE_DIRS})\ninclude_directories(\${CLANG_INCLUDE_DIRS})\nadd_definitions(\${LLVM_DEFINITIONS})\n")
    string(APPEND _CM "add_library(${_TGT} SHARED\n${_SRC_LINES})\n")
    # Link common Clang components; tidy plugins may add their own as needed
    string(APPEND _CM "target_link_libraries(${_TGT} PRIVATE clangAST clangASTMatchers clangBasic clangFrontend clangLex clangTooling)\n")
    string(APPEND _CM "set_target_properties(${_TGT} PROPERTIES OUTPUT_NAME \"${_TGT}\")\n")
  else()
    string(APPEND _CM "add_executable(${_TGT}\n${_SRC_LINES})\n")
  endif()

  # If an existing cache was configured from a different source, reset the dir
  set(_CACHE_FILE "${_BUILD_DIR_GRP}/CMakeCache.txt")
  if(EXISTS "${_CACHE_FILE}")
    file(READ "${_CACHE_FILE}" _cache)
    string(REGEX MATCH "CMAKE_HOME_DIRECTORY:INTERNAL=([^\n]+)" _m "${_cache}")
    if(CMAKE_MATCH_1 AND NOT CMAKE_MATCH_1 STREQUAL "${_BUILD_DIR_GRP}")
      message(STATUS "Resetting build dir for ${_grp} due to source mismatch")
      file(REMOVE_RECURSE "${_BUILD_DIR_GRP}")
      file(MAKE_DIRECTORY "${_BUILD_DIR_GRP}")
    endif()
  endif()

  file(WRITE "${_BUILD_DIR_GRP}/CMakeLists.txt" "${_CM}")

  message(STATUS "\n=== Configuring tool group: ${_grp} ===")
  set(_EXTRA_FLAGS)
  if(DEFINED CMAKE_CXX_FLAGS AND NOT "${CMAKE_CXX_FLAGS}" STREQUAL "")
    list(APPEND _EXTRA_FLAGS -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS})
  endif()
  if(DEFINED CMAKE_EXE_LINKER_FLAGS AND NOT "${CMAKE_EXE_LINKER_FLAGS}" STREQUAL "")
    list(APPEND _EXTRA_FLAGS -DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS})
  endif()
  if(DEFINED CMAKE_SHARED_LINKER_FLAGS AND NOT "${CMAKE_SHARED_LINKER_FLAGS}" STREQUAL "")
    list(APPEND _EXTRA_FLAGS -DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS})
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND}
            -S "${_BUILD_DIR_GRP}" -B "${_BUILD_DIR_GRP}"
            ${_SDK_CFG_FLAG}
            ${_EXTRA_FLAGS}
    RESULT_VARIABLE _cfg_rc
  )
  if(NOT _cfg_rc EQUAL 0)
    message(SEND_ERROR "Configure failed for group ${_grp} (${_cfg_rc})")
    math(EXPR _FAILURES "${_FAILURES}+1")
    continue()
  endif()

  message(STATUS "--- Building group: ${_grp} (parallel ${_NPROC}) ---")
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${_BUILD_DIR_GRP}" --parallel ${_NPROC}
    RESULT_VARIABLE _bld_rc
  )
  if(NOT _bld_rc EQUAL 0)
    message(SEND_ERROR "Build failed for group ${_grp} (${_bld_rc})")
    math(EXPR _FAILURES "${_FAILURES}+1")
  else()
    file(WRITE "${_STAMP_FILE}" "${_LATEST}")
    message(STATUS "✅ Built: ${_grp}")
    # Mark tidy plugin outputs so clang-tidy loader can discover them under clang-plugins dir
    if(_grp MATCHES "^clang-tidy-plugins/")
      file(WRITE "${_BUILD_DIR_GRP}/.is_clang_tidy_plugin" "1")
    endif()
  endif()
endforeach()

if(_FAILURES GREATER 0)
  message(FATAL_ERROR "One or more tool groups failed to build (${_FAILURES}).")
else()
  message(STATUS "All tools built successfully.")
endif()
