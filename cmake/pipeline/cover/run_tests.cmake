# cmake/pipeline/cover/run_tests.cmake
# Run instrumented tests and collect raw profiles

include_guard(GLOBAL)

find_program(CTEST_EXE ctest)
if(NOT CTEST_EXE)
  set(CTEST_EXE "${CMAKE_CTEST_COMMAND}")
endif()

# Profiles directory and merged data
set(PROFILES_DIR "${COV_BIN_DIR}/profiles")
file(MAKE_DIRECTORY "${PROFILES_DIR}")

# Build environment list for cmake -E env
set(_ENV_ARGS)
if(APPLE AND DEFINED CRSCE_MACOS_SDK AND NOT "${CRSCE_MACOS_SDK}" STREQUAL "")
  list(APPEND _ENV_ARGS "SDKROOT=${CRSCE_MACOS_SDK}")
endif()
list(APPEND _ENV_ARGS "LLVM_PROFILE_FILE=${PROFILES_DIR}/%p.profraw")

# Run tests once with profiling enabled (parallel)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env ${_ENV_ARGS} ${CTEST_EXE} --test-dir "${COV_BIN_DIR}" --output-on-failure -j ${_NPROC}
  RESULT_VARIABLE _PR_RC
)
if(NOT _PR_RC EQUAL 0)
  message(FATAL_ERROR "Coverage test run (with profiling) failed (${_PR_RC}).")
endif()

# Create a stable reference path at ${BUILD_DIR}/Temporary pointing to this build's Testing/Temporary
set(_TMP_SRC "${COV_BIN_DIR}/Testing/Temporary")
set(_TMP_DST "${BUILD_DIR}/Temporary")
if(EXISTS "${_TMP_SRC}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f "${_TMP_DST}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${_TMP_SRC}" "${_TMP_DST}")
endif()
