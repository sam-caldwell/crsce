/**
 * @file Sha1HashVerifier_computeHash.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Sha1HashVerifier::computeHash implementation.
 */
#include "decompress/Solvers/Sha1HashVerifier.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "common/BitHashBuffer/sha1/sha1_digest.h"

namespace crsce::decompress::solvers {
    /**
     * @name computeHash
     * @brief Compute SHA-1 over a 512-bit row message (511 data bits + trailing zero).
     * @param row The row data as 8 uint64 words (MSB-first bit ordering).
     * @return 32-byte array with SHA-1 digest in bytes [0..19], zeros in [20..31].
     * @throws None
     */
    auto Sha1HashVerifier::computeHash(const std::array<std::uint64_t, 8> &row) const
        -> std::array<std::uint8_t, 32> {
        // Convert 8 uint64 words to 64 big-endian bytes
        std::array<std::uint8_t, 64> msg{};
        for (int w = 0; w < 8; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        const auto sha1 = crsce::common::detail::sha1::sha1_digest(msg.data(), msg.size());

        // Zero-pad the 20-byte SHA-1 digest to 32 bytes for the IHashVerifier interface
        std::array<std::uint8_t, 32> result{};
        for (std::size_t i = 0; i < kSha1DigestBytes; ++i) {
            result[i] = sha1[i]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }
        return result;
    }
} // namespace crsce::decompress::solvers
