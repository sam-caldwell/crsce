/**
 * @file CompressedPayload_deserializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::deserializeBlock() implementation.
 *
 * B.57: S=127, CRC-32 LH (4 bytes), 2 LTP sub-tables, b=7 bits per uniform element.
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <bit>
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "common/exceptions/DecompressHeaderInvalid.h"
#include "decompress/Solvers/LtpTable.h"

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

        // Bits per uniform cross-sum element: ceil(log2(kS+1)) = bit_width(kS)
        const auto kBitsPerElement = static_cast<std::uint8_t>(
            std::bit_width(static_cast<unsigned>(kS)));

        std::size_t offset = 0;

        // 1. Read kS LH digests (kLHDigestBytes each)
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

        // 4-9. Unpack cross-sum vectors from the bitstream
        std::size_t bitOffset = offset * 8;

        // 4. LSM: kS × b bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            lsm_[k] = unpackBits(data, bitOffset, kBitsPerElement);
        }

        // 5. VSM: kS × b bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            vsm_[k] = unpackBits(data, bitOffset, kBitsPerElement);
        }

        // 6. DSM: kDiagCount variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            dsm_[k] = unpackBits(data, bitOffset, n);
        }

        // 7. XSM: kDiagCount variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            xsm_[k] = unpackBits(data, bitOffset, n);
        }

        // 8. LTP1SM: b bits per element
        for (std::uint16_t k = 0; k < kS; ++k) {
            const auto n = static_cast<std::uint8_t>(
                std::bit_width(decompress::solvers::ltpLineLen(k)));
            ltp1sm_[k] = unpackBits(data, bitOffset, n);
        }

        // 9. LTP2SM: b bits per element
        for (std::uint16_t k = 0; k < kS; ++k) {
            const auto n = static_cast<std::uint8_t>(
                std::bit_width(decompress::solvers::ltpLineLen(k)));
            ltp2sm_[k] = unpackBits(data, bitOffset, n);
        }
    }

} // namespace crsce::common::format
