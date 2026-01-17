# cmake/projects/hello_world.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details


add_executable(hello_world cmd/hello_world/main.cpp)

target_include_directories(hello_world PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

