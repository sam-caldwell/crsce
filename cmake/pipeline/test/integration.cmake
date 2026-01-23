# cmake/pipeline/test/integration.cmake
# Auto-discover and register integration tests in deterministic order

include_guard(GLOBAL)

file(GLOB_RECURSE CRSCE_INTEGRATION_TESTS CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/*/integration/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/integration/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/integration/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/*/integration/*.cpp"
)
list(SORT CRSCE_INTEGRATION_TESTS)
foreach(INTEG_SRC IN LISTS CRSCE_INTEGRATION_TESTS)
  _crsce_add_gtest_from_source("${INTEG_SRC}")
endforeach()
