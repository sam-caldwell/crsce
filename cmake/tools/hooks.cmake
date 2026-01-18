#[[
  File: cmake/tools/hooks.cmake
  Brief: Link git hooks from git-hooks/ into .git/hooks/ using symlinks.
         Usage: cmake -D SOURCE_DIR=<repo_root> -P cmake/tools/hooks.cmake
]]

# Resolve repository root
get_filename_component(_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(_CMAKE_DIR "${_TOOLS_DIR}" DIRECTORY)
get_filename_component(_ROOT_DIR "${_CMAKE_DIR}" DIRECTORY)
if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${_ROOT_DIR}")
endif()

set(GIT_DIR "${SOURCE_DIR}/.git")
set(HOOKS_DIR "${GIT_DIR}/hooks")
set(SRC_HOOKS_DIR "${SOURCE_DIR}/git-hooks")

message(STATUS "--- Linking git hooks ---")
if(NOT EXISTS "${GIT_DIR}")
  message(FATAL_ERROR ".git directory not found. Initialize git first.")
endif()

file(MAKE_DIRECTORY "${HOOKS_DIR}")
file(MAKE_DIRECTORY "${SRC_HOOKS_DIR}")

file(GLOB _HOOKS RELATIVE "${SRC_HOOKS_DIR}" "${SRC_HOOKS_DIR}/*")
set(_count 0)
foreach(_name IN LISTS _HOOKS)
  if(EXISTS "${SRC_HOOKS_DIR}/${_name}" AND NOT IS_DIRECTORY "${SRC_HOOKS_DIR}/${_name}")
    set(_link_target "../../git-hooks/${_name}")
    set(_link_path "${HOOKS_DIR}/${_name}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${_link_target}" "${_link_path}")
    message(STATUS "    link ${_name} -> ${_link_target}")
    math(EXPR _count "${_count} + 1")
  endif()
endforeach()

if(_count EQUAL 0)
  message(STATUS "    No hook files found in git-hooks/.")
endif()

message(STATUS "--- Hooks linking complete ---")
