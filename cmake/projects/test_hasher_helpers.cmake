# Build helper binaries used by hasher tests (not GoogleTest executables)
add_executable(sha256_ok_helper
  ${PROJECT_SOURCE_DIR}/test/cmd/hasher/helpers/sha256_ok.cpp)
target_link_libraries(sha256_ok_helper PRIVATE crsce_static)
target_include_directories(sha256_ok_helper PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(sha256_bad_helper
  ${PROJECT_SOURCE_DIR}/test/cmd/hasher/helpers/sha256_bad.cpp)

# Stage helpers under build/bin for convenience
foreach(_tgt sha256_ok_helper sha256_bad_helper)
  add_custom_command(TARGET ${_tgt} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_tgt}>" "${PROJECT_SOURCE_DIR}/bin/"
    VERBATIM)
endforeach()
