/**
 * @file ConstraintStore_unassign.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::unassign implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/CellState.h"
#include "decompress/Solvers/LtpTable.h"

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

        // Clear assigned bitset (LSB-first for ctzll scanning)
        assigned_[r][c / 64] &= ~(std::uint64_t{1} << (c % 64)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        // Compute flat indices for the 4 original lines
        const auto ri = static_cast<std::uint32_t>(r);
        const auto ci = static_cast<std::uint32_t>(kS) + c;
        const auto di = (2U * kS) + static_cast<std::uint32_t>(c - r + (kS - 1));
        const auto xi = (2U * kS) + kNumDiags + static_cast<std::uint32_t>(r + c);

        // Precomputed flat indices for the 4 slope lines (eliminates expensive % 511)
        const auto &sl = slopeFlatIndices(r, c);
        const auto si0 = static_cast<std::uint32_t>(sl[0]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto si1 = static_cast<std::uint32_t>(sl[1]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto si2 = static_cast<std::uint32_t>(sl[2]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto si3 = static_cast<std::uint32_t>(sl[3]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        // Precomputed flat indices for the 2 LTP lines
        const auto &ltp = ltpFlatIndices(r, c);
        const auto li0 = static_cast<std::uint32_t>(ltp[0]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        const auto li1 = static_cast<std::uint32_t>(ltp[1]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

        // Increment unknown counts for all 10 lines
        stats_[ri].unknown++;
        stats_[ci].unknown++;
        stats_[di].unknown++;
        stats_[xi].unknown++;
        stats_[si0].unknown++;
        stats_[si1].unknown++;
        stats_[si2].unknown++;
        stats_[si3].unknown++;
        stats_[li0].unknown++;
        stats_[li1].unknown++;

        // Decrement assigned counts if was one
        if (wasOne) {
            stats_[ri].assigned--;
            stats_[ci].assigned--;
            stats_[di].assigned--;
            stats_[xi].assigned--;
            stats_[si0].assigned--;
            stats_[si1].assigned--;
            stats_[si2].assigned--;
            stats_[si3].assigned--;
            stats_[li0].assigned--;
            stats_[li1].assigned--;
        }

        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
