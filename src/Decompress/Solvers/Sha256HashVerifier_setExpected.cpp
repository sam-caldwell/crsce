/**
 * @file Sha256HashVerifier_setExpected.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Sha256HashVerifier::setExpected implementation.
 */
#include "decompress/Solvers/Sha256HashVerifier.h"

#include <array>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name setExpected
     * @brief Store the expected digest for a row.
     * @param r Row index.
     * @param digest The 32-byte expected digest.
     * @throws None
     */
    void Sha256HashVerifier::setExpected(const std::uint16_t r,
                                          const std::array<std::uint8_t, 32> &digest) {
        expected_[r] = digest;
    }
} // namespace crsce::decompress::solvers
