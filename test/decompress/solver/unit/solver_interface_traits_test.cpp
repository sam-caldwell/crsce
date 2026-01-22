/**
 * @file solver_interface_traits_test.cpp
 */
#include <gtest/gtest.h>

#include <type_traits>

#include "decompress/Solver/Solver.h"

using crsce::decompress::Solver;

/**

 * @name SolverInterfaceTraits.IsAbstractAndHasVirtualDtor

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(SolverInterfaceTraits, IsAbstractAndHasVirtualDtor) { // NOLINT
    static_assert(std::is_abstract_v<Solver>, "Solver must be abstract");
    static_assert(std::has_virtual_destructor_v<Solver>, "Solver dtor must be virtual");
    SUCCEED();
}
