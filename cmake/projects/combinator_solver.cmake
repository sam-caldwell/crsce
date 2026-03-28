# cmake/projects/combinator_solver.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details.
# B.60: Combinator-algebraic solver CLI tool.

add_executable(combinatorSolver cmd/combinatorSolver/main.cpp)
target_link_libraries(combinatorSolver PRIVATE crsce_static)
add_dependencies(combinatorSolver crsce_static)
