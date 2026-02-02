# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/test_runner_zeroes.cmake

add_executable(testRunnerZeroes cmd/testRunnerZeroes/main.cpp)

target_include_directories(testRunnerZeroes PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(testRunnerZeroes PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(testRunnerZeroes PRIVATE crsce_static)
set_target_properties(testRunnerZeroes PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

add_custom_command(TARGET testRunnerZeroes POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:testRunnerZeroes>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
