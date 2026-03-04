/**
 * @file Sha1HashVerifier_setExpected.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Sha1HashVerifier::setExpected implementation.
 */
#include "decompress/Solvers/Sha1HashVerifier.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::decompress::solvers {
    /**
     * @name setExpected
     * @brief Store the expected SHA-1 digest for a row (only first 20 bytes).
     * @param r Row index.
     * @param digest The 32-byte expected digest (only first 20 bytes are stored).
     * @throws None
     */
    void Sha1HashVerifier::setExpected(const std::uint16_t r,
                                        const std::array<std::uint8_t, 32> &digest) {
        for (std::size_t i = 0; i < kSha1DigestBytes; ++i) {
            expected_[r][i] = digest[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
    }
} // namespace crsce::decompress::solvers
