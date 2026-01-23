# cmake/pipeline/cover/init.cmake
# Initialize coverage environment: paths, toolchain, and SDK helpers

include_guard(GLOBAL)

# Default paths if not provided by caller
if(NOT DEFINED SOURCE_DIR)
  # Derive repo root relative to this file: cmake/pipeline/cover -> repo root
  get_filename_component(_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
  set(SOURCE_DIR "${_REPO_ROOT}")
endif()
if(NOT DEFINED BUILD_DIR)
  set(BUILD_DIR "${SOURCE_DIR}/build")
endif()

# Shared SDK and utility helpers (relative to this file)
include("${CMAKE_CURRENT_LIST_DIR}/../sdk.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../tools/num_cpus.cmake")

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

# Build directory for coverage configuration
set(COV_BIN_DIR "${BUILD_DIR}/arm64-debug-coverage")
