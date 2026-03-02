/**
 * @file ConstraintStore_getCellState.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getCellState implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {
    /**
     * @name getCellState
     * @brief Get the current assignment state of cell (r, c).
     * @param r Row index.
     * @param c Column index.
     * @return The cell's state (Unassigned, Zero, or One).
     * @throws None
     */
    auto ConstraintStore::getCellState(const std::uint16_t r,
                                        const std::uint16_t c) const -> CellState {
        return cells_[(static_cast<std::size_t>(r) * kS) + c];
    }
} // namespace crsce::decompress::solvers
