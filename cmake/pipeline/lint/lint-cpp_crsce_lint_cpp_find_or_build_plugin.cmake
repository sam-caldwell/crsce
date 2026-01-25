#[[
  File: cmake/pipeline/lint/lint-cpp-_crsce_lint_cpp_find_or_build_plugin.cmake
  Brief: Find or build a clang plugin library.
]]
include_guard(GLOBAL)

function(_crsce_lint_cpp_find_or_build_plugin PLUGIN_DIR_NAME OUT_VAR)
  # Always rebuild the plugin to pick up source changes.
  message(STATUS "Building ${PLUGIN_DIR_NAME} plugin (ensure up-to-date)...")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -D SOURCE_DIR=${CMAKE_SOURCE_DIR} -D BUILD_DIR=${CMAKE_SOURCE_DIR}/build -D ONLY_GROUP=clang-plugins/${PLUGIN_DIR_NAME} -D FORCE_REBUILD=ON -P ${CMAKE_SOURCE_DIR}/cmake/pipeline/deps.cmake
    RESULT_VARIABLE _DEP_RC)
  if(NOT _DEP_RC EQUAL 0)
    message(FATAL_ERROR "   Failed to build ${PLUGIN_DIR_NAME} plugin (${_DEP_RC}).")
  endif()

  set(_PLUG_LIB)
  file(GLOB _DYLIB "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/${PLUGIN_DIR_NAME}/*${PLUGIN_DIR_NAME}*.dylib")
  file(GLOB _SO    "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/${PLUGIN_DIR_NAME}/*${PLUGIN_DIR_NAME}*.so")
  file(GLOB _DLL   "${CMAKE_SOURCE_DIR}/build/tools/clang-plugins/${PLUGIN_DIR_NAME}/*${PLUGIN_DIR_NAME}*.dll")
  if(_DYLIB)
    list(GET _DYLIB 0 _PLUG_LIB)
  elseif(_SO)
    list(GET _SO 0 _PLUG_LIB)
  elseif(_DLL)
    list(GET _DLL 0 _PLUG_LIB)
  endif()
  if(NOT _PLUG_LIB)
    message(FATAL_ERROR "   ${PLUGIN_DIR_NAME} plugin library not found after build.")
  endif()
  set(${OUT_VAR} "${_PLUG_LIB}" PARENT_SCOPE)
endfunction()
