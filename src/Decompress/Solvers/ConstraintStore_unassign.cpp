/**
 * @file ConstraintStore_unassign.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::unassign implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {
    /**
     * @name unassign
     * @brief Revert the assignment of cell (r, c) to unassigned, restoring line statistics.
     * @param r Row index.
     * @param c Column index.
     * @throws None
     */
    void ConstraintStore::unassign(const std::uint16_t r, const std::uint16_t c) {
        const auto wasOne = (cells_[(static_cast<std::size_t>(r) * kS) + c] == CellState::One);
        cells_[(static_cast<std::size_t>(r) * kS) + c] = CellState::Unassigned;

        // Clear row bit if it was set
        if (wasOne) {
            const auto word = c / 64;
            const auto bit = 63 - (c % 64);
            rowBits_[r][word] &= ~(static_cast<std::uint64_t>(1) << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // Update row stats
        rowStats_[r].unknown++;
        if (wasOne) { rowStats_[r].assigned--; }

        // Update column stats
        colStats_[c].unknown++;
        if (wasOne) { colStats_[c].assigned--; }

        // Update diagonal stats
        const auto d = static_cast<std::uint16_t>(c - r + (kS - 1));
        diagStats_[d].unknown++;
        if (wasOne) { diagStats_[d].assigned--; }

        // Update anti-diagonal stats
        const auto x = static_cast<std::uint16_t>(r + c);
        antiDiagStats_[x].unknown++;
        if (wasOne) { antiDiagStats_[x].assigned--; }
    }
} // namespace crsce::decompress::solvers
