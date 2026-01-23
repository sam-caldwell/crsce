# cmake/pipeline/test/e2e.cmake
# Auto-discover and register end-to-end tests in deterministic order

include_guard(GLOBAL)

file(GLOB_RECURSE CRSCE_E2E_TESTS CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/*/e2e/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/e2e/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/e2e/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/*/e2e/*.cpp"
)
list(SORT CRSCE_E2E_TESTS)
foreach(E2E_SRC IN LISTS CRSCE_E2E_TESTS)
  _crsce_add_gtest_from_source("${E2E_SRC}")
endforeach()
