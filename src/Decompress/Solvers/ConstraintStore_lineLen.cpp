/**
 * @file ConstraintStore_lineLen.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::lineLen implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <algorithm>
#include <cstdint>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name lineLen
     * @brief Compute the length of a line (number of cells).
     * @param line The line identifier.
     * @return Number of cells on the line.
     * @throws None
     */
    auto ConstraintStore::lineLen(const LineID line) const -> std::uint16_t {
        switch (line.type) {
            case LineType::Row:
            case LineType::Column:
            case LineType::Slope256:
            case LineType::Slope255:
            case LineType::Slope2:
            case LineType::Slope509:
                return kS;
            case LineType::Diagonal:
            case LineType::AntiDiagonal: {
                const auto k = static_cast<int>(line.index);
                return static_cast<std::uint16_t>(
                    std::min({k + 1, static_cast<int>(kS), static_cast<int>(kNumDiags) - k}));
            }
        }
        return 0;
    }
} // namespace crsce::decompress::solvers
