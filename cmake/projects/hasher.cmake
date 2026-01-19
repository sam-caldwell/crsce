# cmake/projects/hasher.cmake

add_executable(hasher
        ${PROJECT_SOURCE_DIR}/cmd/hasher/main.cpp
)

target_link_libraries(hasher PRIVATE crsce_static)
target_include_directories(hasher PRIVATE "${PROJECT_SOURCE_DIR}/include")
