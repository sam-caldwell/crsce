/**
 * @file ConstraintStore_getRowUnknownCount.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getRowUnknownCount implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name getRowUnknownCount
     * @brief Get the unknown count for a specific row.
     * @param r Row index.
     * @return The unknown count for row r.
     * @throws None
     */
    auto ConstraintStore::getRowUnknownCount(const std::uint16_t r) const -> std::uint16_t {
        return rowStats_[r].unknown;
    }
} // namespace crsce::decompress::solvers
