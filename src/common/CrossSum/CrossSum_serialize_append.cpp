/**
 * @file CrossSum_serialize_append.cpp
 * @brief Serialize CrossSum elements as contiguous 9-bit MSB-first bitstream.
 */
#include "common/CrossSum/CrossSum.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crsce::common {
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
        if (bit_pos != 0) { // GCOVR_EXCL_BR_LINE (kSize*kBitWidth=4599 -> remainder 7 bits; else branch unreachable)
            out.push_back(curr);
        }
    }
} // namespace crsce::common
