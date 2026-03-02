/**
 * @file Sha256HashVerifier_computeHash.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Sha256HashVerifier::computeHash implementation.
 */
#include "decompress/Solvers/Sha256HashVerifier.h"

#include <array>
#include <cstdint>

#include "common/BitHashBuffer/sha256/sha256_digest.h"

namespace crsce::decompress::solvers {
    /**
     * @name computeHash
     * @brief Compute SHA-256 over a 512-bit row message (511 data bits + trailing zero).
     * @param row The row data as 8 uint64 words (MSB-first bit ordering).
     * @return 32-byte SHA-256 digest.
     * @throws None
     */
    auto Sha256HashVerifier::computeHash(const std::array<std::uint64_t, 8> &row) const
        -> std::array<std::uint8_t, 32> {
        // Convert 8 uint64 words to 64 big-endian bytes
        std::array<std::uint8_t, 64> msg{};
        for (int w = 0; w < 8; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        return crsce::common::detail::sha256::sha256_digest(msg.data(), msg.size());
    }
} // namespace crsce::decompress::solvers
