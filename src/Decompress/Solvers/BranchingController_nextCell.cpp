/**
 * @file BranchingController_nextCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief BranchingController::nextCell implementation.
 */
#include "decompress/Solvers/BranchingController.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "decompress/Solvers/CellState.h"
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

        for (std::uint16_t r = minUnassignedRow_; r < kS; ++r) {
            // Skip rows with no unknowns
            if (cs.getRowUnknownCount(r) == 0) {
                continue;
            }
            for (std::uint16_t c = 0; c < kS; ++c) {
                if (cs.getCellState(r, c) == CellState::Unassigned) {
                    minUnassignedRow_ = r;
                    return std::pair{r, c};
                }
            }
        }
        return std::nullopt;
    }
} // namespace crsce::decompress::solvers
