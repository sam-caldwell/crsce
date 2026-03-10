/**
 * @file ConstraintStore_getRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief ConstraintStore::getRow implementation.
 */
#include "decompress/Solvers/ConstraintStore.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name getRow
     * @brief Extract the current row as 8 uint64 words for hash verification.
     * @param r Row index.
     * @return Const reference to the row data as 8 uint64 words with MSB-first bit ordering.
     * @throws None
     */
    auto ConstraintStore::getRow(const std::uint16_t r) const -> const std::array<std::uint64_t, 8> & {
        return rowBits_[r];
    }
} // namespace crsce::decompress::solvers
