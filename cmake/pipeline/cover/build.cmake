# cmake/pipeline/cover/build.cmake
# Build the coverage-instrumented binaries

include_guard(GLOBAL)

crsce_num_cpus(_NPROC)
if(NOT _NPROC)
  set(_NPROC 1)
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${COV_BIN_DIR}" --parallel ${_NPROC}
  RESULT_VARIABLE _BLD_RC
)
if(NOT _BLD_RC EQUAL 0)
  message(FATAL_ERROR "Coverage build step failed (${_BLD_RC}).")
endif()
