#[[
  File: cmake/pipeline/cover.cmake
  Brief: Build with GCC/gcov, run CTest, and enforce a coverage threshold.

  Usage:
    cmake -D BUILD_DIR=<build_root> -D SOURCE_DIR=<repo_root> -P cmake/pipeline/cover.cmake

  Notes:
    - Prefers Homebrew gcc (e.g., g++-15) on macOS.
    - Fails if only clang is available (coverage requires gcov).
]]

if(NOT DEFINED BUILD_DIR)
  set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
endif()
if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR "${CMAKE_SOURCE_DIR}")
endif()

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

# Discover macOS SDK for Homebrew LLVM if available
set(_SDK_PATH "")
find_program(XCRUN_EXE xcrun)
if(XCRUN_EXE)
  execute_process(COMMAND ${XCRUN_EXE} --show-sdk-path OUTPUT_VARIABLE _SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CXX=${CLANGXX_EXE} CC=${CLANGC_EXE}
          ${CMAKE_COMMAND}
            -S ${SOURCE_DIR} -B ${COV_BIN_DIR}
            -G Ninja
          -DCMAKE_BUILD_TYPE=Debug
          -DCMAKE_CXX_STANDARD=23
          -DCMAKE_CXX_FLAGS=-fprofile-instr-generate\ -fcoverage-mapping
          -DCMAKE_EXE_LINKER_FLAGS=-fprofile-instr-generate
          $<$<BOOL:${_SDK_PATH}>:-DCMAKE_OSX_SYSROOT=${_SDK_PATH}>
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
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${PROFILES_DIR}/%p.profraw
          ${CTEST_EXE} --test-dir "${COV_BIN_DIR}" --output-on-failure
  RESULT_VARIABLE _PR_RC
)
if(NOT _PR_RC EQUAL 0)
  message(FATAL_ERROR "Coverage test run (with profiling) failed (${_PR_RC}).")
endif()

# Merge profiles
set(PROFDATA "${COV_BIN_DIR}/coverage.profdata")
file(GLOB RAW_PROFILES "${PROFILES_DIR}/*.profraw")
if(RAW_PROFILES)
  execute_process(COMMAND ${LLVM_PROFDATA_EXE} merge -sparse ${RAW_PROFILES} -o ${PROFDATA}
                  RESULT_VARIABLE _PD_RC)
  if(NOT _PD_RC EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge failed (${_PD_RC}).")
  endif()
else()
  message(FATAL_ERROR "No .profraw files produced; ensure tests executed.")
endif()

# Identify instrumented binaries to include in coverage
set(COV_BINARIES)
foreach(name IN ITEMS compress decompress hello_world test_common_argparser_unit_argparser_unit test_common_filebitserializer_unit_filebitserializer_unit)
  if(EXISTS "${COV_BIN_DIR}/${name}")
    list(APPEND COV_BINARIES "${COV_BIN_DIR}/${name}")
  endif()
endforeach()
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
message(STATUS "Coverage TOTAL: ${_PCT}% (threshold ${_THRESHOLD}%)")

# Compare as tenths of a percent to avoid float math
string(REGEX REPLACE "([0-9]+)\.([0-9]).*" "\\1\\2" _PCT_TENTHS "${_PCT}")
if(_PCT_TENTHS STREQUAL "${_PCT}")
  set(_PCT_TENTHS "${_PCT}0")
endif()
math(EXPR _THRESHOLD_TENTHS "${_THRESHOLD} * 10")
if(_PCT_TENTHS LESS _THRESHOLD_TENTHS)
  message(FATAL_ERROR "Coverage threshold not met: ${_PCT}% < ${_THRESHOLD}%\n${_RPT}")
endif()

message(STATUS "✅ Coverage check complete")
