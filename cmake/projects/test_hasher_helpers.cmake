# Build helper binaries used by hasher tests (not GoogleTest executables)
add_executable(sha256_ok_helper
  ${PROJECT_SOURCE_DIR}/test/cmd/hasher/helpers/sha256_ok.cpp)
target_link_libraries(sha256_ok_helper PRIVATE crsce_static)
target_include_directories(sha256_ok_helper PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(sha256_bad_helper
  ${PROJECT_SOURCE_DIR}/test/cmd/hasher/helpers/sha256_bad.cpp)
