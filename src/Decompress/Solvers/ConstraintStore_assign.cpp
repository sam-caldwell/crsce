/**
 * @file ConstraintStore_assign.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::assign implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"

namespace crsce::decompress::solvers {
    /**
     * @name assign
     * @brief Assign a value (0 or 1) to cell (r, c), updating line statistics and row bits.
     * @param r Row index.
     * @param c Column index.
     * @param v Value to assign (0 or 1).
     * @throws None
     */
    void ConstraintStore::assign(const std::uint16_t r, const std::uint16_t c, const std::uint8_t v) {
        cells_[(static_cast<std::size_t>(r) * kS) + c] = (v != 0) ? CellState::One : CellState::Zero;

        // Update row bits (MSB-first: column c maps to word c/64, bit 63-(c%64))
        if (v != 0) {
            const auto word = c / 64;
            const auto bit = 63 - (c % 64);
            rowBits_[r][word] |= (static_cast<std::uint64_t>(1) << bit); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // Update row stats
        rowStats_[r].unknown--;
        if (v != 0) { rowStats_[r].assigned++; }

        // Update column stats
        colStats_[c].unknown--;
        if (v != 0) { colStats_[c].assigned++; }

        // Update diagonal stats: d = c - r + (kS - 1)
        const auto d = static_cast<std::uint16_t>(c - r + (kS - 1));
        diagStats_[d].unknown--;
        if (v != 0) { diagStats_[d].assigned++; }

        // Update anti-diagonal stats: x = r + c
        const auto x = static_cast<std::uint16_t>(r + c);
        antiDiagStats_[x].unknown--;
        if (v != 0) { antiDiagStats_[x].assigned++; }
    }
} // namespace crsce::decompress::solvers
