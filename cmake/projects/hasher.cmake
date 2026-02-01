# cmake/projects/hasher.cmake

add_executable(hasher
        ${PROJECT_SOURCE_DIR}/cmd/hasher/main.cpp
)

target_link_libraries(hasher PRIVATE crsce_static)
target_include_directories(hasher PRIVATE "${PROJECT_SOURCE_DIR}/include")

# Stage under build/bin
add_custom_command(TARGET hasher POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:hasher>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
