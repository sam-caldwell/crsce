/**
 * @file ConstraintStore_getCellValue.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getCellValue implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {
    /**
     * @name getCellValue
     * @brief Get the assigned value of cell (r, c).
     * @param r Row index.
     * @param c Column index.
     * @return 0 or 1. Returns 0 for unassigned cells.
     * @throws None
     */
    auto ConstraintStore::getCellValue(const std::uint16_t r,
                                        const std::uint16_t c) const -> std::uint8_t {
        return (cells_[(static_cast<std::size_t>(r) * kS) + c] == CellState::One) ? 1 : 0;
    }
} // namespace crsce::decompress::solvers
