/**
 * @file ConstraintStore_slopeFlatIndices.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::slopeFlatIndices -- precomputed slope line index table.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/CrossSum/ToroidalSlopeSum.h"

namespace crsce::decompress::solvers {
    /**
     * @name slopeFlatIndices
     * @brief Return precomputed flat stat indices for the 4 slope partitions at cell (r, c).
     *
     * Eliminates expensive modular arithmetic (% 511 involves integer division) from
     * hot paths by precomputing a 511x511x4 lookup table (~2MB) on first access.
     * The table is a function-local static, guaranteeing thread-safe one-time initialization.
     *
     * @param r Row index in [0, kS).
     * @param c Column index in [0, kS).
     * @return Reference to array of 4 flat stat indices [slope256, slope255, slope2, slope509].
     */
    auto ConstraintStore::slopeFlatIndices(const std::uint16_t r, const std::uint16_t c)
        -> const std::array<std::uint16_t, kNumSlopePartitions> & {
        using Table = std::array<std::array<std::uint16_t, kNumSlopePartitions>,
                                 static_cast<std::size_t>(kS) * kS>;
        static const Table table = [] {
            Table t{};
            for (std::uint16_t row = 0; row < kS; ++row) {
                for (std::uint16_t col = 0; col < kS; ++col) {
                    auto &entry = t.at((static_cast<std::size_t>(row) * kS) + col);
                    entry.at(0) = static_cast<std::uint16_t>(
                        kSlope256Base + common::ToroidalSlopeSum::slopeLineIndex(row, col, kSlopes.at(0), kS));
                    entry.at(1) = static_cast<std::uint16_t>(
                        kSlope255Base + common::ToroidalSlopeSum::slopeLineIndex(row, col, kSlopes.at(1), kS));
                    entry.at(2) = static_cast<std::uint16_t>(
                        kSlope2Base + common::ToroidalSlopeSum::slopeLineIndex(row, col, kSlopes.at(2), kS));
                    entry.at(3) = static_cast<std::uint16_t>(
                        kSlope509Base + common::ToroidalSlopeSum::slopeLineIndex(row, col, kSlopes.at(3), kS));
                }
            }
            return t;
        }();
        return table.at((static_cast<std::size_t>(r) * kS) + c);
    }
} // namespace crsce::decompress::solvers
