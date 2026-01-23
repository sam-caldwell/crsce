# cmake/pipeline/cover/configure.cmake
# Configure a coverage-instrumented build tree

include_guard(GLOBAL)

# Clean existing coverage build dir for a fresh configure
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
            -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=${BUILD_DIR}/llvm-debug/_deps/googletest-src
            ${_SDK_CFG_FLAG}
  RESULT_VARIABLE _CFG_RC
)
if(NOT _CFG_RC EQUAL 0)
  message(FATAL_ERROR "Coverage configure step failed (${_CFG_RC}).")
endif()
