/**
 * @file Sha1HashVerifier_verifyRow.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 * @brief Sha1HashVerifier::verifyRow implementation (CRC-32 for B.57).
 */
#include "decompress/Solvers/Sha1HashVerifier.h"

#include <array>
#include <cstdint>

#include "common/Util/crc32_ieee.h"

namespace crsce::decompress::solvers {
    /**
     * @name verifyRow
     * @brief Verify that a fully-assigned row matches its expected CRC-32 digest.
     * @param r Row index.
     * @param row The row data as 2 uint64 words.
     * @return True if the computed CRC-32 hash matches the stored expected digest.
     * @throws None
     */
    auto Sha1HashVerifier::verifyRow(const std::uint16_t r,
                                      const std::array<std::uint64_t, 2> &row) const -> bool {
        // Convert 2 uint64 words to 16 big-endian bytes
        std::array<std::uint8_t, 16> msg{};
        for (int w = 0; w < 2; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        const std::uint32_t crc = crsce::common::util::crc32_ieee(msg.data(), msg.size());
        const std::array<std::uint8_t, kSha1DigestBytes> computed = {
            static_cast<std::uint8_t>((crc >> 24U) & 0xFFU),
            static_cast<std::uint8_t>((crc >> 16U) & 0xFFU),
            static_cast<std::uint8_t>((crc >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(crc & 0xFFU)
        };
        return computed == expected_[r]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
} // namespace crsce::decompress::solvers
