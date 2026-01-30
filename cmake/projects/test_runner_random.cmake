# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_random.cmake

add_executable(testRunnerRandom cmd/testRunnerRandom/main.cpp)

target_include_directories(testRunnerRandom PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerRandom PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerRandom PRIVATE crsce_static)
