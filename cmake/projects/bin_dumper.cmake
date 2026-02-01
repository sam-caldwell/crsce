# cmake/projects/bin_dumper.cmake

add_executable(binDumper cmd/binDumper/main.cpp)

target_include_directories(binDumper PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(binDumper PRIVATE crsce_static)

# Also place a copy under build/tools/ for convenience (preset-agnostic path)
add_custom_command(TARGET binDumper POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/../tools"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:binDumper>" "${CMAKE_BINARY_DIR}/../tools/binDumper"
  VERBATIM
)

# Also stage under build/bin
add_custom_command(TARGET binDumper POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:binDumper>" "${PROJECT_SOURCE_DIR}/bin/"
  VERBATIM)
