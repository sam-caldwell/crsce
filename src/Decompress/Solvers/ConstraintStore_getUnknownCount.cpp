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
        return stats_[lineIndex(line)].unknown; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
