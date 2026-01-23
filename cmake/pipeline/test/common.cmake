# cmake/pipeline/test/common.cmake
# Shared helpers for test discovery/registration

include_guard(GLOBAL)

# Helper to create a test binary from a single source and register it with CTest
function(_crsce_add_gtest_from_source SRC)
  file(RELATIVE_PATH REL_PATH "${PROJECT_SOURCE_DIR}/test" "${SRC}")
  string(REPLACE "/" "_" TGT_NAME "${REL_PATH}")
  string(REPLACE ".cpp" "" TGT_NAME "${TGT_NAME}")
  set(TGT "test_${TGT_NAME}")

  add_executable(${TGT} "${SRC}")
  target_include_directories(${TGT} PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/test")
  target_compile_definitions(${TGT} PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")
  target_link_libraries(${TGT} PRIVATE crsce_static GTest::gtest GTest::gtest_main)
  add_test(NAME ${TGT} COMMAND ${TGT})
endfunction()
