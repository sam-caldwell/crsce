/**
 * @file ConstraintStore_getUnknownCount.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getUnknownCount implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name getUnknownCount
     * @brief Get the count of unassigned cells on a line.
     * @param line The line identifier.
     * @return The unknown count u(L).
     * @throws None
     */
    auto ConstraintStore::getUnknownCount(const LineID line) const -> std::uint16_t {
        switch (line.type) {
            case LineType::Row:         return rowStats_[line.index].unknown;
            case LineType::Column:      return colStats_[line.index].unknown;
            case LineType::Diagonal:    return diagStats_[line.index].unknown;
            case LineType::AntiDiagonal: return antiDiagStats_[line.index].unknown;
        }
        return 0;
    }
} // namespace crsce::decompress::solvers
