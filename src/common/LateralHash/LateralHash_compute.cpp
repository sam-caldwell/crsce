/**
 * @file LateralHash_compute.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief LateralHash::compute() implementation.
 */
#include "common/LateralHash/LateralHash.h"

#include <array>
#include <cstdint>

#include "common/BitHashBuffer/sha1/sha1_digest.h"

namespace crsce::common {
    /**
     * @name compute
     * @brief Compute the SHA-1 digest of a 511-bit row (plus trailing zero = 512 bits = 64 bytes).
     * @details Converts the 8 uint64 words to 64 bytes in big-endian order (MSB of each uint64 first),
     * then delegates to sha1_digest.  The 511 data bits fill the first 511 bit positions and
     * the 512th bit (bit 0 of word 7) is the trailing zero, ensuring byte alignment.
     * @param row The 8 x uint64_t words representing the row, MSB-first bit ordering.
     * @return 20-byte SHA-1 digest.
     * @throws None
     */
    std::array<std::uint8_t, LateralHash::kDigestBytes>
    LateralHash::compute(const std::array<std::uint64_t, 8> &row) {
        std::array<std::uint8_t, 64> msg{};
        for (int w = 0; w < 8; ++w) {
            for (int b = 7; b >= 0; --b) {
                msg[(w * 8) + (7 - b)] = static_cast<std::uint8_t>(row[w] >> (b * 8)); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            }
        }
        return detail::sha1::sha1_digest(msg.data(), msg.size());
    }
} // namespace crsce::common
