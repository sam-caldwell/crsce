# cmake/pipeline/cover/find_binaries.cmake
# Locate instrumented binaries and libraries for coverage mapping

include_guard(GLOBAL)

set(COV_BINARIES)

# Project binaries
foreach(name IN ITEMS compress decompress hello_world hasher)
  if(EXISTS "${COV_BIN_DIR}/${name}")
    list(APPEND COV_BINARIES "${COV_BIN_DIR}/${name}")
  endif()
endforeach()

# All unit/integration/e2e test executables (one-per-file pattern)
file(GLOB TEST_EXES "${COV_BIN_DIR}/test_*")
foreach(exe IN LISTS TEST_EXES)
  if(EXISTS "${exe}")
    list(APPEND COV_BINARIES "${exe}")
  endif()
endforeach()

# Also include the static library for robust mapping discovery (macOS/LLVM)
if(EXISTS "${COV_BIN_DIR}/libcrsce_static.a")
  list(APPEND COV_BINARIES "${COV_BIN_DIR}/libcrsce_static.a")
endif()

list(REMOVE_DUPLICATES COV_BINARIES)
if(NOT COV_BINARIES)
  message(FATAL_ERROR "No instrumented binaries found for coverage report.")
endif()
