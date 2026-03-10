/**
 * @file Csm_vec.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Csm::vec() implementation.
 */
#include "common/Csm/Csm.h"

#include <cstdint>
#include <vector>

namespace crsce::common {
    /**
     * @name vec
     * @brief Serialize the matrix to a row-major packed bitstring.
     * @details Iterates over all kS rows, extracting kS bits from each
     * row in MSB-first order and packing them into consecutive bytes
     * (also MSB-first within each byte).  The total output length is
     * ceil(kS * kS / 8) bytes.
     * @return Packed byte vector.
     * @throws None
     */
    auto Csm::vec() const -> std::vector<std::uint8_t> {
        constexpr std::uint32_t totalBits = static_cast<std::uint32_t>(kS) * kS;
        constexpr std::uint32_t totalBytes = (totalBits + 7) / 8;
        std::vector<std::uint8_t> out(totalBytes, 0);

        std::uint32_t bitIdx = 0;
        for (std::uint16_t r = 0; r < kS; ++r) {
            for (std::uint16_t c = 0; c < kS; ++c) {
                const auto word = static_cast<std::uint16_t>(c / 64);
                const auto bit = static_cast<std::uint16_t>(63 - (c % 64));
                const auto val = static_cast<std::uint8_t>((rows_[r][word] >> bit) & 1ULL); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)

                if (val != 0) {
                    const std::uint32_t byteIdx = bitIdx / 8;
                    const auto bitPos = static_cast<std::uint8_t>(7 - (bitIdx % 8));
                    out[byteIdx] |= static_cast<std::uint8_t>(1U << bitPos);
                }
                ++bitIdx;
            }
        }
        return out;
    }
} // namespace crsce::common
