/**
 * @file RowHashVerifier_pack_row_bytes.cpp
 * @brief Implementation of RowHashVerifier::pack_row_bytes.
 * @author Sam Caldwell
 * @copyright © 2026 Sam Caldwell. See LICENSE.txt for details
 */
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/Csm.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace crsce::decompress {
    /**
     * @name RowHashVerifier::pack_row_bytes
     * @brief Pack row r bits into a big-endian byte array of size kRowSize (64 bytes).
     * @param csm Source CSM.
     * @param r Row index to pack.
     * @return 64-byte array representing the row bits (MSB-first in each byte).
     */
    std::array<std::uint8_t, RowHashVerifier::kRowSize>
    RowHashVerifier::pack_row_bytes(const Csm &csm, const std::size_t r) {
        std::array<std::uint8_t, kRowSize> row{};
        std::size_t byte_idx = 0;
        int bit_pos = 0; // 0..7; 0 = MSB position
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
        // After 511 bits, flush the partial byte; remaining lowest bit is pad 0 (already zero)
        if (bit_pos != 0) { row.at(byte_idx) = curr; }
        return row;
    }
}
