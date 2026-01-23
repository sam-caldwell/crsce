# cmake/pipeline/cover/merge_profiles.cmake
# Merge raw profiles into a single .profdata

include_guard(GLOBAL)

set(PROFDATA "${COV_BIN_DIR}/coverage.profdata")

# Prefer profiles from the dedicated profiles/ dir; fallback only if empty
file(GLOB RAW_PROFILES "${PROFILES_DIR}/*.profraw")
if(RAW_PROFILES)
  set(ALL_PROFILES ${RAW_PROFILES})
else()
  file(GLOB_RECURSE RAW_PROFILES_FALLBACK "${COV_BIN_DIR}/*.profraw")
  set(ALL_PROFILES ${RAW_PROFILES_FALLBACK})
endif()
list(REMOVE_DUPLICATES ALL_PROFILES)
if(ALL_PROFILES)
  execute_process(
    COMMAND ${LLVM_PROFDATA_EXE} merge -sparse ${ALL_PROFILES} -o ${PROFDATA}
    RESULT_VARIABLE _PD_RC
  )
  if(NOT _PD_RC EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge failed (${_PD_RC}).")
  endif()
else()
  message(FATAL_ERROR "No .profraw files produced; ensure tests executed and binaries are instrumented.")
endif()
