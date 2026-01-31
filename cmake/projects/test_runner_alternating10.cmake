# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_alternating10.cmake

add_executable(testRunnerAlternating10 cmd/testRunnerAlternating10/main.cpp)

target_include_directories(testRunnerAlternating10 PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerAlternating10 PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerAlternating10 PRIVATE crsce_static)
