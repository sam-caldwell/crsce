/**
 * @file ConstraintStore_getLinesForCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getLinesForCell implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <array>
#include <cstdint>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name getLinesForCell
     * @brief Get the four LineIDs (row, col, diag, anti-diag) that cell (r, c) participates in.
     * @param r Row index.
     * @param c Column index.
     * @return Array of 4 LineIDs.
     * @throws None
     */
    auto ConstraintStore::getLinesForCell(const std::uint16_t r,
                                          const std::uint16_t c) const -> std::array<LineID, 4> {
        return {{
            {.type = LineType::Row, .index = r},
            {.type = LineType::Column, .index = c},
            {.type = LineType::Diagonal, .index = static_cast<std::uint16_t>(c - r + (kS - 1))},
            {.type = LineType::AntiDiagonal, .index = static_cast<std::uint16_t>(r + c)}
        }};
    }
} // namespace crsce::decompress::solvers
