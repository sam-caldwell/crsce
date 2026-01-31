# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_alternating01.cmake

add_executable(testRunnerAlternating01 cmd/testRunnerAlternating01/main.cpp)

target_include_directories(testRunnerAlternating01 PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerAlternating01 PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerAlternating01 PRIVATE crsce_static)
