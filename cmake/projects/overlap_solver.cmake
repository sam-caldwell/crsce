# cmake/projects/overlap_solver.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details.
# B.61a: Overlapping block solver CLI tool.

add_executable(overlapSolver cmd/overlapSolver/main.cpp)
target_link_libraries(overlapSolver PRIVATE crsce_static)
add_dependencies(overlapSolver crsce_static)
