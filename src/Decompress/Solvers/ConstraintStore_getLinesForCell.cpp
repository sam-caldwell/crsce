/**
 * @file ConstraintStore_getLinesForCell.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getLinesForCell implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>

#include "decompress/Solvers/IConstraintStore.h"
#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {
    /**
     * @name getLinesForCell
     * @brief Get the LineIDs (row, col, diag, anti-diag, LTP1–LTP4) that cell (r, c) participates in.
     *
     * B.22: always returns 8 lines (4 basic + 4 LTP, one per sub-table, full coverage).
     *
     * @param r Row index.
     * @param c Column index.
     * @return CellLines with always 8 valid LineIDs.
     * @throws None
     */
    auto ConstraintStore::getLinesForCell(const std::uint16_t r,
                                          const std::uint16_t c) const -> CellLines {
        const auto &mem = ltpMembership(r, c);

        CellLines result;
        result.lines[0] = {.type = LineType::Row,          .index = r};
        result.lines[1] = {.type = LineType::Column,        .index = c};
        result.lines[2] = {.type = LineType::Diagonal,      .index = static_cast<std::uint16_t>(c - r + (kS - 1))};
        result.lines[3] = {.type = LineType::AntiDiagonal,  .index = static_cast<std::uint16_t>(r + c)};

        // B.22: always 4 LTP lines (one per sub-table, full coverage)
        for (std::uint8_t j = 0; j < mem.count; ++j) {
            const auto flatIdx = static_cast<std::uint32_t>(mem.flat[j]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            result.lines[4 + j] = flatIndexToLineID(flatIdx); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        result.count = static_cast<std::uint8_t>(4U + mem.count);

        return result;
    }
} // namespace crsce::decompress::solvers
