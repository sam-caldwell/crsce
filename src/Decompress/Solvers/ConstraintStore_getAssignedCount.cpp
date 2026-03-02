/**
 * @file ConstraintStore_getAssignedCount.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getAssignedCount implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>

#include "decompress/Solvers/LineID.h"

namespace crsce::decompress::solvers {
    /**
     * @name getAssignedCount
     * @brief Get the count of assigned-one cells on a line.
     * @param line The line identifier.
     * @return The assigned-ones count a(L).
     * @throws None
     */
    auto ConstraintStore::getAssignedCount(const LineID line) const -> std::uint16_t {
        return stats_[lineIndex(line)].assigned; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
