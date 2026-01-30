# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_zeros.cmake

add_executable(testRunnerZeros cmd/testRunnerZeros/main.cpp)

target_include_directories(testRunnerZeros PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerZeros PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerZeros PRIVATE crsce_static)
