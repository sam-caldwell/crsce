/**
 * @file Sha256HashVerifier_verifyRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Sha256HashVerifier::verifyRow implementation.
 */
#include "decompress/Solvers/Sha256HashVerifier.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name verifyRow
     * @brief Verify that a fully-assigned row matches its expected digest.
     * @param r Row index.
     * @param row The row data as 8 uint64 words.
     * @return True if the computed hash matches the stored expected digest.
     * @throws None
     */
    auto Sha256HashVerifier::verifyRow(const std::uint16_t r,
                                        const std::array<std::uint64_t, 2> &row) const -> bool {
        return computeHash(row) == expected_[r];
    }
} // namespace crsce::decompress::solvers
