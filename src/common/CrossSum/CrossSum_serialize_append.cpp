/**
 * @file CrossSum_serialize_append.cpp
 * @brief Serialize CrossSum elements as contiguous 9-bit MSB-first bitstream.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @name serialize_append
     * @brief Serialize all cross-sum elements as a contiguous 9-bit MSB-first bitstream.
     * @param out Destination byte vector; 575 bytes are appended (ceil(511*9/8)).
     * @return N/A
     * @throws None
     * @details
     * - Each of the 511 values is clamped to 9 bits and emitted MSB-first.
     * - Bits are packed into bytes with bit 7 first; a final partial byte is emitted
     *   when necessary. For 511 elements at 9 bits each (4599 bits), this appends 575 bytes.
     */
    void CrossSum::serialize_append(std::vector<std::uint8_t> &out) const {
        std::uint8_t curr = 0;
        int bit_pos = 0; // next bit position in curr [0..7], 0 = MSB

        for (std::size_t i = 0; i < kSize; ++i) {
            // Keep only 9 bits per spec
            const auto v = static_cast<std::uint16_t>(elems_.at(i) & 0x01FFU);
            for (int b = static_cast<int>(kBitWidth) - 1; b >= 0; --b) {
                if (const bool bit = ((v >> b) & 0x1U); bit != 0U) {
                    // Set bit at position (7 - bit_pos)
                    // NOLINTNEXTLINE(readability-magic-numbers)
                    curr |= static_cast<std::uint8_t>(1U << (7 - bit_pos));
                }
                ++bit_pos;
                // NOLINTNEXTLINE(readability-magic-numbers)
                if (bit_pos >= 8) {
                    out.push_back(curr);
                    curr = 0;
                    bit_pos = 0;
                }
            }
        }
        if (bit_pos != 0) {
            // GCOVR_EXCL_BR_LINE (kSize*kBitWidth=4599 -> remainder 7 bits; else branch unreachable)
            out.push_back(curr);
        }
    }
} // namespace crsce::common
