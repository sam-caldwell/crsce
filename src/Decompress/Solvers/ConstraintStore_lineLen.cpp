/**
 * @file ConstraintStore_lineLen.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::lineLen implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <algorithm>
#include <cstdint>

#include "decompress/Solvers/LineID.h"
#include "decompress/Solvers/LtpTable.h"

namespace crsce::decompress::solvers {
    /**
     * @name lineLen
     * @brief Compute the length of a line (number of cells).
     *
     * B.21: LTP lines have variable length ltp_len(k) = min(k+1, kS-k), not always kS.
     *
     * @param line The line identifier.
     * @return Number of cells on the line.
     * @throws None
     */
    auto ConstraintStore::lineLen(const LineID line) const -> std::uint16_t {
        switch (line.type) {
            case LineType::Row:
            case LineType::Column:
                return kS;
            case LineType::LTP1:
            case LineType::LTP2:
            case LineType::LTP3:
            case LineType::LTP4:
                return ltpLineLen(line.index);
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
