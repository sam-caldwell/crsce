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

# Attempt to build instrumented clang plugins (ODPH/ODPCPP etc.) under the coverage build dir
execute_process(
  COMMAND ${CMAKE_COMMAND}
          -D SOURCE_DIR=${SOURCE_DIR}
          -D BUILD_DIR=${COV_BIN_DIR}
          -D CMAKE_CXX_FLAGS=-fprofile-instr-generate\ -fcoverage-mapping
          -D CMAKE_SHARED_LINKER_FLAGS=-fprofile-instr-generate
          -P ${CMAKE_CURRENT_LIST_DIR}/../deps.cmake
  RESULT_VARIABLE _DEPS_RC
)
if(NOT _DEPS_RC EQUAL 0)
  message(WARNING "Coverage: building instrumented plugins failed; continuing without plugin coverage.")
endif()

# Discover instrumented plugin libraries to include in coverage mapping
file(GLOB_RECURSE _PLUGIN_LIBS
  "${COV_BIN_DIR}/tools/clang-plugins/*/*.dylib"
  "${COV_BIN_DIR}/tools/clang-plugins/*/*.so"
)
foreach(lib IN LISTS _PLUGIN_LIBS)
  if(EXISTS "${lib}")
    list(APPEND COV_BINARIES "${lib}")
  endif()
endforeach()

list(REMOVE_DUPLICATES COV_BINARIES)
if(NOT COV_BINARIES)
  message(FATAL_ERROR "No instrumented binaries found for coverage report.")
endif()
