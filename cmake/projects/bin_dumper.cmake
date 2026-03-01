# cmake/projects/bin_dumper.cmake

add_executable(binDumper cmd/binDumper/main.cpp)

target_include_directories(binDumper PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(binDumper PRIVATE crsce_static)

# Also stage under build/bin
add_custom_command(TARGET binDumper POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:binDumper>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
