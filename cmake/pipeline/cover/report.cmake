# cmake/pipeline/cover/report.cmake
# Generate coverage report, annotate key sources, and enforce threshold

include_guard(GLOBAL)

# Quick diagnostic: show annotated coverage for FileBitSerializer.cpp (first match)
set(_FBSRC "${SOURCE_DIR}/src/common/FileBitSerializer.cpp")
if(EXISTS "${_FBSRC}")
  foreach(_BIN IN LISTS COV_BINARIES)
    execute_process(
      COMMAND ${LLVM_COV_EXE} show -instr-profile=${PROFDATA} ${_BIN} --sources ${_FBSRC}
      OUTPUT_VARIABLE _SHOW_OUT
      RESULT_VARIABLE _SHOW_RC
    )
    if(_SHOW_RC EQUAL 0 AND _SHOW_OUT)
      message(STATUS "\n--- Annotated: src/common/FileBitSerializer.cpp (from ${_BIN}) ---\n${_SHOW_OUT}")
      break()
    endif()
  endforeach()
endif()

# Generate coverage report and enforce threshold
# Ignore OS-level exec/pipe helper and thin CLI sources for stable signal
set(_IGNORE_REGEX "(RunSha256Stdin\\.cpp|ComputeControlSha256\\.cpp|/cmd/.*\\.cpp)")

if(NOT DEFINED COVERAGE_THRESHOLD)
  if(DEFINED _THRESHOLD)
    set(COVERAGE_THRESHOLD ${_THRESHOLD})
  else()
    set(COVERAGE_THRESHOLD 95)
  endif()
endif()

execute_process(
  COMMAND ${LLVM_COV_EXE} report -instr-profile=${PROFDATA} -ignore-filename-regex=${_IGNORE_REGEX} ${COV_BINARIES}
  WORKING_DIRECTORY ${SOURCE_DIR}
  OUTPUT_VARIABLE _RPT
  RESULT_VARIABLE _RC_RPT
)
if(NOT _RC_RPT EQUAL 0)
  message(FATAL_ERROR "llvm-cov report failed (${_RC_RPT}).")
endif()

# Extract TOTAL line percentage
string(REGEX REPLACE ".*TOTAL[^\n]*%[^\n]*% ([0-9]+\.?[0-9]*)%.*" "\\1" _PCT_LINES "${_RPT}")
if(NOT _PCT_LINES)
  string(REGEX REPLACE ".*TOTAL[^\n]* ([0-9]+\.?[0-9]*)%.*" "\\1" _PCT_LINES "${_RPT}")
endif()
set(_PCT ${_PCT_LINES})
if(NOT _PCT)
  message(FATAL_ERROR "Failed to parse coverage percentage from llvm-cov output:\n${_RPT}")
endif()

message(STATUS "\n--- Coverage Report ---\n${_RPT}")
message(STATUS "Coverage TOTAL: ${_PCT}% (threshold ${COVERAGE_THRESHOLD}%)")

# Compare as tenths of a percent to avoid float math
string(REGEX REPLACE "([0-9]+)\.([0-9]).*" "\\1\\2" _PCT_TENTHS "${_PCT}")
if(_PCT_TENTHS STREQUAL "${_PCT}")
  set(_PCT_TENTHS "${_PCT}0")
endif()
math(EXPR _THRESHOLD_TENTHS "${COVERAGE_THRESHOLD} * 10")
if(_PCT_TENTHS LESS _THRESHOLD_TENTHS)
  message(FATAL_ERROR "Coverage threshold not met: ${_PCT}% < ${COVERAGE_THRESHOLD}%")
endif()

message(STATUS "✅ Coverage check complete")
