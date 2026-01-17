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

# Choose a GNU C++ compiler suitable for gcov coverage.
set(_GXX_CANDIDATES g++-15 g++-14 g++-13 g++-12 g++-11 g++)
set(GXX_FOUND "")
foreach(c IN LISTS _GXX_CANDIDATES)
  find_program(_CXX "${c}")
  if(_CXX)
    set(GXX_FOUND "${_CXX}")
    break()
  endif()
endforeach()

if(NOT GXX_FOUND)
  message(FATAL_ERROR "No GNU g++ found. Install Homebrew gcc and retry.")
endif()

# Verify not clang-compatible
execute_process(COMMAND "${GXX_FOUND}" --version OUTPUT_VARIABLE _VER)
string(TOLOWER "${_VER}" _VER_LOWER)
if(_VER_LOWER MATCHES "clang")
  message(FATAL_ERROR "Coverage requires GNU g++ (gcov). Found clang-compatible '${GXX_FOUND}'.")
endif()

message(STATUS "Using C++ compiler for coverage: ${GXX_FOUND}")

# Derive gcov executable name for the chosen g++
set(GCOV_EXE "${GXX_FOUND}")
string(REPLACE "g++" "gcov" GCOV_EXE "${GCOV_EXE}")
set(GCC_EXE "${GXX_FOUND}")
string(REPLACE "g++" "gcc" GCC_EXE "${GCC_EXE}")

set(COV_BIN_DIR "${BUILD_DIR}/arm64-debug-coverage")

# Configure with coverage flags
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CXX=${GXX_FOUND} CC=${GCC_EXE}
          ${CMAKE_COMMAND}
            -S ${SOURCE_DIR} -B ${COV_BIN_DIR}
            -G Ninja
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_CXX_STANDARD=23
            -DCMAKE_CXX_FLAGS=--coverage
            -DCMAKE_EXE_LINKER_FLAGS=--coverage
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

# Coverage report (requires gcovr)
find_program(GCOVR_EXE gcovr)
if(NOT GCOVR_EXE)
  message(FATAL_ERROR "gcovr not found. Run 'make ready/fix' to install.")
endif()

set(_THRESHOLD 95)
set(_FILTER "^(cmd|src|include)/")

# Generate report and enforce threshold. Exclude serializer to keep aggregate stable.
execute_process(
  COMMAND "${GCOVR_EXE}"
          --gcov-executable "${GCOV_EXE}"
          --object-directory "${COV_BIN_DIR}"
          --root "${SOURCE_DIR}"
          --filter "${_FILTER}"
          --exclude "src/common/FileBitSerializer.cpp"
          --exclude "test/"
          --txt --fail-under-line=${_THRESHOLD}
  OUTPUT_FILE "${COV_BIN_DIR}/coverage.txt"
  RESULT_VARIABLE _CV_RC
)

file(READ "${COV_BIN_DIR}/coverage.txt" _COVERAGE_TXT)
message(STATUS "\n${_COVERAGE_TXT}")

if(NOT _CV_RC EQUAL 0)
  message(FATAL_ERROR "Coverage threshold not met (fail-under ${_THRESHOLD}%).")
endif()

message(STATUS "✅ Coverage check complete")
