/**
 * @file BranchingController_nextCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::nextCell implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "decompress/Solvers/ConstraintStore.h"

namespace crsce::decompress::solvers {
    /**
     * @name nextCell
     * @brief Find the first unassigned cell in row-major order.
     * @return Pair (r, c) of the next cell, or nullopt if all cells are assigned.
     * @throws None
     */
    auto BranchingController::nextCell() const
        -> std::optional<std::pair<std::uint16_t, std::uint16_t>> {
        // Devirtualize: store_ is guaranteed to be ConstraintStore (final class)
        const auto &cs = static_cast<const ConstraintStore &>(store_); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)

        // Delegate to bitset-based scan (O(1) per word via ctzll)
        auto result = cs.getFirstUnassigned(minUnassignedRow_);
        if (result.has_value()) {
            minUnassignedRow_ = result->first;
        }
        return result;
    }
} // namespace crsce::decompress::solvers
