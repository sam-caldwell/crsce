# cmake/pipeline/test/tlt.cmake
# Auto-discover and register top-level tests (future work) in deterministic order

include_guard(GLOBAL)

file(GLOB CRSCE_TLT_TESTS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/test/tlt/*.cpp")
list(SORT CRSCE_TLT_TESTS)
foreach(TLT_SRC IN LISTS CRSCE_TLT_TESTS)
  _crsce_add_gtest_from_source("${TLT_SRC}")
endforeach()
