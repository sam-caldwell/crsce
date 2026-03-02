/**
 * @file CompressedPayload_deserializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::deserializeBlock() implementation.
 *
 * Deserializes a kBlockPayloadBytes-byte buffer back into a CompressedPayload,
 * reading LH digests, the DI byte, and four bit-packed cross-sum vectors.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <cstring>

#include "common/exceptions/DecompressHeaderInvalid.h"

namespace crsce::common::format {

    /**
     * @name deserializeBlock
     * @brief Deserialize a kBlockPayloadBytes-byte buffer into this payload.
     * @details
     * Reads 511 x 32-byte LH digests, the DI byte, then unpacks the four
     * cross-sum vectors from the MSB-first bitstream.  LSM and VSM are 9 bits
     * per element; DSM and XSM are variable-width per element.
     * @param data Pointer to at least kBlockPayloadBytes bytes.
     * @param len Length of the buffer (must be >= kBlockPayloadBytes).
     * @throws DecompressHeaderInvalid if len < kBlockPayloadBytes.
     */
    void CompressedPayload::deserializeBlock(const std::uint8_t *data, const std::size_t len) {
        if (len < kBlockPayloadBytes) {
            throw exceptions::DecompressHeaderInvalid("CompressedPayload::deserializeBlock: buffer too small");
        }

        std::size_t offset = 0;

        // 1. Read 511 LH digests (32 bytes each)
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::memcpy(lh_[r].data(), data + offset, 32); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            offset += 32;
        }

        // 2. Read DI byte
        di_ = data[offset]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        offset += 1;

        // 3-6. Unpack cross-sum vectors from the bitstream
        std::size_t bitOffset = offset * 8;

        // 3. LSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            lsm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 4. VSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            vsm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 5. DSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            dsm_[k] = unpackBits(data, bitOffset, n);
        }

        // 6. XSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            xsm_[k] = unpackBits(data, bitOffset, n);
        }
    }

} // namespace crsce::common::format
