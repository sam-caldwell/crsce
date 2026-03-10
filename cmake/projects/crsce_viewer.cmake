# cmake/projects/crsce_viewer.cmake

add_executable(crsce_viewer cmd/crsce_viewer/main.cpp)

target_include_directories(crsce_viewer PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(crsce_viewer PRIVATE crsce_static)

# Also stage under build/bin
add_custom_command(TARGET crsce_viewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:crsce_viewer>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
