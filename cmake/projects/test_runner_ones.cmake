# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_ones.cmake

add_executable(testRunnerOnes cmd/testRunnerOnes/main.cpp)

target_include_directories(testRunnerOnes PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerOnes PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerOnes PRIVATE crsce_static)
