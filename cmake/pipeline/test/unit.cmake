# cmake/pipeline/test/unit.cmake
# Auto-discover and register unit tests in deterministic order

include_guard(GLOBAL)

file(GLOB_RECURSE CRSCE_UNIT_TESTS CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/test/*/unit/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/unit/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/unit/*.cpp"
  "${PROJECT_SOURCE_DIR}/test/*/*/*/*/unit/*.cpp"
)
list(SORT CRSCE_UNIT_TESTS)
foreach(UNIT_SRC IN LISTS CRSCE_UNIT_TESTS)
  _crsce_add_gtest_from_source("${UNIT_SRC}")
endforeach()
