# (c) 2026 Sam Caldwell.
# See LICENSE.txt for details
#
# cmake/projects/useless_test.cmake

add_executable(uselessTest cmd/uselessTest/main.cpp)

target_include_directories(uselessTest PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(uselessTest PRIVATE TEST_BINARY_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(uselessTest PRIVATE crsce_static)

set_target_properties(uselessTest PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET uselessTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:uselessTest>" "${CMAKE_BINARY_DIR}/bin/"
  VERBATIM)
