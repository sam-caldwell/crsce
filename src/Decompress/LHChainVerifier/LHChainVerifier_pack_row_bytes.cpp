/**
 * @file LHChainVerifier_pack_row_bytes.cpp
 * @brief Implementation of LHChainVerifier::pack_row_bytes.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/detail/Csm.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name LHChainVerifier::pack_row_bytes
     * @brief Pack row r bits plus 1 pad bit into a 64-byte buffer (MSB-first per byte).
     * @param csm Cross-Sum Matrix providing row bits.
     * @param r Row index [0..510].
     * @return std::array<std::uint8_t, kRowSize> Packed row bytes.
     */
    std::array<std::uint8_t, LHChainVerifier::kRowSize>
    LHChainVerifier::pack_row_bytes(const Csm &csm, const std::size_t r) {
        std::array<std::uint8_t, kRowSize> row{};
        std::size_t byte_idx = 0;
        int bit_pos = 0; // 0..7; 0 is MSB position
        std::uint8_t curr = 0;
        for (std::size_t c = 0; c < kS; ++c) {
            const bool b = csm.get(r, c);
            if (b) {
                curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos)));
            }
            ++bit_pos;
            if (bit_pos >= 8) {
                row.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        // After 511 bits, flush the partial byte; the remaining lowest bit is the pad 0 (already zero)
        if (bit_pos != 0) {
            row.at(byte_idx) = curr;
        }
        return row;
    }
} // namespace crsce::decompress
