# cmake/projects/row_solver.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details.
# B.59h: Row-serial solver CLI tool.

add_executable(rowSolver cmd/rowSolver/main.cpp)
target_link_libraries(rowSolver PRIVATE crsce_static)
add_dependencies(rowSolver crsce_static)
