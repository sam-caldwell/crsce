# cmake/projects/bin_dumper.cmake

add_executable(binDumper cmd/binDumper/main.cpp)

target_include_directories(binDumper PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)

target_link_libraries(binDumper PRIVATE crsce_static)

