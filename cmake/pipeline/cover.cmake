#[[
  File: cmake/pipeline/cover.cmake
  Brief: Build with GCC/gcov, run CTest, and enforce a coverage threshold.

  Usage:
    cmake -D BUILD_DIR=<build_root> -D SOURCE_DIR=<repo_root> -P cmake/pipeline/cover.cmake

  Notes:
    - Uses Homebrew clang/llvm tooling for source-based coverage (llvm-profdata/llvm-cov).
    - Instruments with -fprofile-instr-generate and -fcoverage-mapping.
]]

if(NOT DEFINED BUILD_DIR)
  set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
endif()
if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${CMAKE_SOURCE_DIR}")
endif()

include("${CMAKE_SOURCE_DIR}/cmake/pipeline/sdk.cmake")

# Choose clang/llvm tools for source-based coverage
find_program(CLANGXX_EXE clang++ HINTS /opt/homebrew/opt/llvm/bin)
find_program(CLANGC_EXE clang HINTS /opt/homebrew/opt/llvm/bin)
find_program(LLVM_PROFDATA_EXE llvm-profdata HINTS /opt/homebrew/opt/llvm/bin)
find_program(LLVM_COV_EXE llvm-cov HINTS /opt/homebrew/opt/llvm/bin)
if(NOT CLANGXX_EXE OR NOT CLANGC_EXE)
  message(FATAL_ERROR "clang/clang++ not found. Run 'make ready/fix' to install llvm.")
endif()
if(NOT LLVM_PROFDATA_EXE OR NOT LLVM_COV_EXE)
  message(FATAL_ERROR "llvm-profdata/llvm-cov not found. Run 'make ready/fix' to install llvm.")
endif()

execute_process(COMMAND "${CLANGXX_EXE}" --version OUTPUT_VARIABLE _CLVER)
message(STATUS "Using clang++ for coverage: ${CLANGXX_EXE}\n${_CLVER}")

set(COV_BIN_DIR "${BUILD_DIR}/arm64-debug-coverage")

# Configure with coverage flags
if(EXISTS "${COV_BIN_DIR}")
  file(REMOVE_RECURSE "${COV_BIN_DIR}")
endif()

crsce_sdk_configure_args(_SDK_CFG_FLAG)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CXX=${CLANGXX_EXE} CC=${CLANGC_EXE}
          ${CMAKE_COMMAND}
            -S ${SOURCE_DIR} -B ${COV_BIN_DIR}
            -G Ninja
          -DCMAKE_BUILD_TYPE=Debug
          -DCMAKE_CXX_STANDARD=23
          -DCMAKE_CXX_FLAGS=-fprofile-instr-generate\ -fcoverage-mapping
          -DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate
          ${_SDK_CFG_FLAG}
  RESULT_VARIABLE _CFG_RC
)
if(NOT _CFG_RC EQUAL 0)
  message(FATAL_ERROR "Coverage configure step failed (${_CFG_RC}).")
endif()

# Build
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${COV_BIN_DIR}" RESULT_VARIABLE _BLD_RC)
if(NOT _BLD_RC EQUAL 0)
  message(FATAL_ERROR "Coverage build step failed (${_BLD_RC}).")
endif()

# Test
find_program(CTEST_EXE ctest)
if(NOT CTEST_EXE)
  set(CTEST_EXE "${CMAKE_CTEST_COMMAND}")
endif()
execute_process(
  COMMAND "${CTEST_EXE}" --test-dir "${COV_BIN_DIR}" --output-on-failure
  RESULT_VARIABLE _TST_RC
)
if(NOT _TST_RC EQUAL 0)
  message(FATAL_ERROR "Coverage test step failed (${_TST_RC}).")
endif()

# Coverage report using llvm-profdata and llvm-cov

# Profiles directory and merged data
set(PROFILES_DIR "${COV_BIN_DIR}/profiles")
file(MAKE_DIRECTORY "${PROFILES_DIR}")

# Run tests with profile output enabled
crsce_sdk_env_prefix(_SDK_ENV_PREFIX)
if(_SDK_ENV_PREFIX)
  set(_SDK_ENV_PREFIX "${_SDK_ENV_PREFIX} ")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env ${_SDK_ENV_PREFIX}LLVM_PROFILE_FILE=${PROFILES_DIR}/%p.profraw
          ${CTEST_EXE} --test-dir "${COV_BIN_DIR}" --output-on-failure
  RESULT_VARIABLE _PR_RC
)
if(NOT _PR_RC EQUAL 0)
  message(FATAL_ERROR "Coverage test run (with profiling) failed (${_PR_RC}).")
endif()

# Merge profiles (search broadly to be robust to runner working directories)
set(PROFDATA "${COV_BIN_DIR}/coverage.profdata")
file(GLOB RAW_PROFILES "${PROFILES_DIR}/*.profraw")
file(GLOB_RECURSE RAW_PROFILES_FALLBACK "${COV_BIN_DIR}/*.profraw")
set(ALL_PROFILES ${RAW_PROFILES} ${RAW_PROFILES_FALLBACK})
list(REMOVE_DUPLICATES ALL_PROFILES)
if(ALL_PROFILES)
  execute_process(COMMAND ${LLVM_PROFDATA_EXE} merge -sparse ${ALL_PROFILES} -o ${PROFDATA}
                  RESULT_VARIABLE _PD_RC)
  if(NOT _PD_RC EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge failed (${_PD_RC}).")
  endif()
else()
  message(FATAL_ERROR "No .profraw files produced; ensure tests executed and binaries are instrumented.")
endif()

# Identify instrumented binaries to include in coverage
set(COV_BINARIES)
# Project binaries
foreach(name IN ITEMS compress decompress hello_world)
  if(EXISTS "${COV_BIN_DIR}/${name}")
    list(APPEND COV_BINARIES "${COV_BIN_DIR}/${name}")
  endif()
endforeach()
# All unit test executables (one-per-file pattern)
file(GLOB TEST_EXES "${COV_BIN_DIR}/test_*")
foreach(exe IN LISTS TEST_EXES)
  if(EXISTS "${exe}")
    list(APPEND COV_BINARIES "${exe}")
  endif()
endforeach()
list(REMOVE_DUPLICATES COV_BINARIES)
if(NOT COV_BINARIES)
  message(FATAL_ERROR "No instrumented binaries found for coverage report.")
endif()

# Generate coverage report and enforce threshold
set(_THRESHOLD 95)
execute_process(
  COMMAND ${LLVM_COV_EXE} report -instr-profile=${PROFDATA} ${COV_BINARIES}
  WORKING_DIRECTORY ${SOURCE_DIR}
  OUTPUT_VARIABLE _RPT
  RESULT_VARIABLE _RC_RPT
)
if(NOT _RC_RPT EQUAL 0)
  message(FATAL_ERROR "llvm-cov report failed (${_RC_RPT}).")
endif()

# Extract TOTAL line percentage
string(REGEX REPLACE ".*TOTAL[^\n]* ([0-9]+\.?[0-9]*)%.*" "\\1" _PCT "${_RPT}")
if(NOT _PCT)
  message(FATAL_ERROR "Failed to parse coverage percentage from llvm-cov output:\n${_RPT}")
endif()
message(STATUS "\n--- Coverage Report ---\n${_RPT}")
message(STATUS "Coverage TOTAL: ${_PCT}% (threshold ${_THRESHOLD}%)")

# Compare as tenths of a percent to avoid float math
string(REGEX REPLACE "([0-9]+)\.([0-9]).*" "\\1\\2" _PCT_TENTHS "${_PCT}")
if(_PCT_TENTHS STREQUAL "${_PCT}")
  set(_PCT_TENTHS "${_PCT}0")
endif()
math(EXPR _THRESHOLD_TENTHS "${_THRESHOLD} * 10")
if(_PCT_TENTHS LESS _THRESHOLD_TENTHS)
  # Print the error after the report and summary so the failure appears last
  message(FATAL_ERROR "Coverage threshold not met: ${_PCT}% < ${_THRESHOLD}%")
endif()

message(STATUS "✅ Coverage check complete")
