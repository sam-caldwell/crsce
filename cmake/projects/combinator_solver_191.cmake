# cmake/projects/combinator_solver_191.cmake
# (c) 2026 Sam Caldwell. See LICENSE.txt for details.
# B.62: Combinator solver at S=191.
# Standalone binary: compiles CombinatorSolver.cpp + main.cpp with CRSCE_S=191.
# Links crsce_static for shared utilities (Csm, BlockHash, CRC, etc.) but
# the CombinatorSolver symbols come from the local compilation, not the library.

add_executable(combinatorSolver191
    cmd/combinatorSolver/main.cpp
    src/Decompress/Solvers/CombinatorSolver.cpp
)
target_include_directories(combinatorSolver191 PRIVATE include)
target_compile_definitions(combinatorSolver191 PRIVATE CRSCE_S=191)
target_link_libraries(combinatorSolver191 PRIVATE crsce_static)
