/**
 * @file CompressedPayload_deserializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::deserializeBlock() implementation.
 *
 * Deserializes a kBlockPayloadBytes-byte buffer back into a CompressedPayload,
 * reading LH digests, BH digest, the DI byte, and eight bit-packed cross-sum vectors.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <cstring>

#include "common/exceptions/DecompressHeaderInvalid.h"

namespace crsce::common::format {

    /**
     * @name deserializeBlock
     * @brief Deserialize a kBlockPayloadBytes-byte buffer into this payload.
     * @param data Pointer to at least kBlockPayloadBytes bytes.
     * @param len Length of the buffer (must be >= kBlockPayloadBytes).
     * @throws DecompressHeaderInvalid if len < kBlockPayloadBytes.
     */
    void CompressedPayload::deserializeBlock(const std::uint8_t *data, const std::size_t len) {
        if (len < kBlockPayloadBytes) {
            throw exceptions::DecompressHeaderInvalid("CompressedPayload::deserializeBlock: buffer too small");
        }

        std::size_t offset = 0;

        // 1. Read 511 LH digests (20 bytes each)
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::memcpy(lh_[r].data(), data + offset, kLHDigestBytes); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            offset += kLHDigestBytes;
        }

        // 2. Read BH digest (32 bytes)
        std::memcpy(bh_.data(), data + offset, kBHDigestBytes); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        offset += kBHDigestBytes;

        // 3. Read DI byte
        di_ = data[offset]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        offset += 1;

        // 4-11. Unpack cross-sum vectors from the bitstream
        std::size_t bitOffset = offset * 8;

        // 4. LSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            lsm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 5. VSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            vsm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 6. DSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            dsm_[k] = unpackBits(data, bitOffset, n);
        }

        // 7. XSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            xsm_[k] = unpackBits(data, bitOffset, n);
        }

        // 8. LTP1SM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            ltp1sm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 9. LTP2SM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            ltp2sm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 10. LTP3SM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            ltp3sm_[k] = unpackBits(data, bitOffset, 9);
        }

        // 11. LTP4SM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            ltp4sm_[k] = unpackBits(data, bitOffset, 9);
        }
    }

} // namespace crsce::common::format
