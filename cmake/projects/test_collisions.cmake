# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_collisions.cmake

add_executable(testCollisions cmd/testCollisions/main.cpp)

target_include_directories(testCollisions PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testCollisions PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testCollisions PRIVATE crsce_static)

# Place executable under build/bin to avoid clashing with output directory
set_target_properties(testCollisions PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
