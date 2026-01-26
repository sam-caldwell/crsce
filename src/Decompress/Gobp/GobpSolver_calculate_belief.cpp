/**
 * @file GobpSolver_calculate_belief.cpp
 * @brief simple helper function to calculate belief
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */

#include "decompress/Gobp/GobpSolver.h"
#include <cstddef>
#include <cstdint> //NOLINT

namespace crsce::decompress {
    /**
     * @name GobpSolver::calculate_belief
     * @brief calculate the belief for belief_for()
     * @param R element coordinate
     * @param U element coordinate
     * @return double Belief value in [0,1].
     */
    auto GobpSolver::calculate_belief(const std::size_t R, const std::size_t U) -> double {
        if (U == 0U) {
            return 0.5;
        }
        if (R == 0U) {
            return 0.0;
        }
        if (R == U) {
            return 1.0;
        }
        return static_cast<double>(R) / static_cast<double>(U);
    };
}
