/**
 * @file ConstraintStore_getLinesForCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getLinesForCell implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <array>
#include <cstdint>

#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {
    /**
     * @name getLinesForCell
     * @brief Get the ten LineIDs (row, col, diag, anti-diag, 4 slopes, LTP1, LTP2) that cell (r, c) participates in.
     * @param r Row index.
     * @param c Column index.
     * @return Array of 10 LineIDs.
     * @throws None
     */
    auto ConstraintStore::getLinesForCell(const std::uint16_t r,
                                          const std::uint16_t c) const -> std::array<LineID, 10> {
        const auto &sl = slopeFlatIndices(r, c);
        const auto &ltp = ltpFlatIndices(r, c);
        return {{
            {.type = LineType::Row, .index = r},
            {.type = LineType::Column, .index = c},
            {.type = LineType::Diagonal, .index = static_cast<std::uint16_t>(c - r + (kS - 1))},
            {.type = LineType::AntiDiagonal, .index = static_cast<std::uint16_t>(r + c)},
            {.type = LineType::Slope256, .index = static_cast<std::uint16_t>(sl[0] - kSlope256Base)}, // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            {.type = LineType::Slope255, .index = static_cast<std::uint16_t>(sl[1] - kSlope255Base)}, // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            {.type = LineType::Slope2,   .index = static_cast<std::uint16_t>(sl[2] - kSlope2Base)},   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            {.type = LineType::Slope509, .index = static_cast<std::uint16_t>(sl[3] - kSlope509Base)}, // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            {.type = LineType::LTP1,     .index = static_cast<std::uint16_t>(ltp[0] - kLTP1Base)},    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            {.type = LineType::LTP2,     .index = static_cast<std::uint16_t>(ltp[1] - kLTP2Base)}     // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }};
    }
} // namespace crsce::decompress::solvers
