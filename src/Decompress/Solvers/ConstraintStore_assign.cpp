/**
 * @file ConstraintStore_assign.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::assign implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/LtpTable.h"

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

        // Update assigned bitset (LSB-first for ctzll scanning)
        assigned_[r][c / 64] |= (std::uint64_t{1} << (c % 64)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        // Compute flat indices for the 4 basic lines
        const auto ri = static_cast<std::uint32_t>(r);
        const auto ci = static_cast<std::uint32_t>(kS) + c;
        const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
        const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

        // Precomputed flat indices for the 4 LTP lines
        const auto &ltp = ltpFlatIndices(r, c);
        const auto li0 = static_cast<std::uint32_t>(ltp[0]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto li1 = static_cast<std::uint32_t>(ltp[1]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto li2 = static_cast<std::uint32_t>(ltp[2]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto li3 = static_cast<std::uint32_t>(ltp[3]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

        // Decrement unknown counts for all 8 lines
        stats_[ri].unknown--;
        stats_[ci].unknown--;
        stats_[di].unknown--;
        stats_[xi].unknown--;
        stats_[li0].unknown--;
        stats_[li1].unknown--;
        stats_[li2].unknown--;
        stats_[li3].unknown--;

        // Increment assigned counts if value is 1
        if (v != 0) {
            stats_[ri].assigned++;
            stats_[ci].assigned++;
            stats_[di].assigned++;
            stats_[xi].assigned++;
            stats_[li0].assigned++;
            stats_[li1].assigned++;
            stats_[li2].assigned++;
            stats_[li3].assigned++;
        }

        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
