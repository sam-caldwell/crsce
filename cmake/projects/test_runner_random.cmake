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
set_target_properties(testRunnerRandom PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

# Stage under build/bin
add_custom_command(TARGET testRunnerRandom POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:testRunnerRandom>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
