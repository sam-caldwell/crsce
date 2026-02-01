# cmake/projects/hello_world.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details


add_executable(hello_world cmd/hello_world/main.cpp)

target_include_directories(hello_world PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

# Stage under build/bin
add_custom_command(TARGET hello_world POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:hello_world>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
