/**
 * @file CompressedPayload_serializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::serializeBlock() implementation.
 *
 * Serializes one compressed block into exactly kBlockPayloadBytes (19,549) bytes:
 *   1. 511 x 32-byte LH digests              (16,352 bytes)
 *   2. 1-byte DI                              (1 byte)
 *   3. LSM: 511 x 9 bits, MSB-first packed    (4,599 bits)
 *   4. VSM: 511 x 9 bits, MSB-first packed    (4,599 bits)
 *   5. DSM: 1021 variable-width, MSB-first    (8,185 bits)
 *   6. XSM: 1021 variable-width, MSB-first    (8,185 bits)
 *   Total cross-sum region: 25,568 bits = 3,196 bytes (no partial -- evenly divisible)
 *   Grand total: 16,352 + 1 + 3,196 = 19,549 bytes
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace crsce::common::format {

    /**
     * @name serializeBlock
     * @brief Serialize this payload into a kBlockPayloadBytes-byte buffer.
     * @details
     * LH digests are written sequentially (511 x 32 bytes).  The DI byte follows.
     * The four cross-sum vectors are then bit-packed MSB-first into a contiguous
     * bitstream.  LSM and VSM use 9 bits per element (511 elements each).
     * DSM and XSM use ceil(log2(len(k)+1)) bits per element (1021 elements each),
     * where len(k) = min(k+1, 511, 1021-k).  The final partial byte (if any) is
     * zero-padded on the right.
     * @return Vector of exactly kBlockPayloadBytes bytes.
     * @throws None
     */
    std::vector<std::uint8_t> CompressedPayload::serializeBlock() const {
        std::vector<std::uint8_t> buf(kBlockPayloadBytes, 0);
        std::size_t offset = 0;

        // 1. Write 511 LH digests (32 bytes each)
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::memcpy(buf.data() + offset, lh_[r].data(), 32); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            offset += 32;
        }

        // 2. Write DI byte
        buf[offset] = di_;
        offset += 1;

        // 3-6. Bit-pack cross-sum vectors starting at byte offset
        // All bits are packed MSB-first into the remaining buffer space.
        std::size_t bitOffset = offset * 8;

        // 3. LSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, lsm_[k], 9);
        }

        // 4. VSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, vsm_[k], 9);
        }

        // 5. DSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, dsm_[k], n);
        }

        // 6. XSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, xsm_[k], n);
        }

        // Any remaining bits in the final byte are already zero (buffer was zero-initialized).
        return buf;
    }

} // namespace crsce::common::format
