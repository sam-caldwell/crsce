/**
 * @file BranchingController_branchOrder.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::branchOrder implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name branchOrder
     * @brief Return the canonical branching order {0, 1} for lexicographic enumeration.
     * @return Array of bit values in trial order: 0 first, then 1.
     * @throws None
     */
    auto BranchingController::branchOrder() const -> std::array<std::uint8_t, 2> {
        return {0, 1};
    }
} // namespace crsce::decompress::solvers
