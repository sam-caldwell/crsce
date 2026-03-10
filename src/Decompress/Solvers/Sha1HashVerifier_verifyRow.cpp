/**
 * @file Sha1HashVerifier_verifyRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Sha1HashVerifier::verifyRow implementation.
 */
#include "decompress/Solvers/Sha1HashVerifier.h"

#include <array>
#include <cstdint>

#include "common/BitHashBuffer/sha1/sha1_digest.h"

namespace crsce::decompress::solvers {
    /**
     * @name verifyRow
     * @brief Verify that a fully-assigned row matches its expected SHA-1 digest.
     * @param r Row index.
     * @param row The row data as 8 uint64 words.
     * @return True if the computed SHA-1 hash matches the stored expected digest (first 20 bytes).
     * @throws None
     */
    auto Sha1HashVerifier::verifyRow(const std::uint16_t r,
                                      const std::array<std::uint64_t, 8> &row) const -> bool {
        // Convert 8 uint64 words to 64 big-endian bytes
        std::array<std::uint8_t, 64> msg{};
        for (int w = 0; w < 8; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        const auto computed = crsce::common::detail::sha1::sha1_digest(msg.data(), msg.size());
        return computed == expected_[r];
    }
} // namespace crsce::decompress::solvers
