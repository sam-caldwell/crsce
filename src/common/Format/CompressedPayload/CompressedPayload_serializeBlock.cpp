/**
 * @file CompressedPayload_serializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::serializeBlock() implementation.
 *
 * B.57: S=127, CRC-32 LH (4 bytes), 2 LTP sub-tables, b=7 bits per uniform element.
 * Payload: 127×4 LH + 32 BH + 1 DI + bit-packed (LSM+VSM+DSM+XSM+LTP1+LTP2).
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "decompress/Solvers/LtpTable.h"

namespace crsce::common::format {

    /**
     * @name serializeBlock
     * @brief Serialize this payload into a kBlockPayloadBytes-byte buffer.
     * @return Vector of exactly kBlockPayloadBytes bytes.
     * @throws None
     */
    std::vector<std::uint8_t> CompressedPayload::serializeBlock() const {
        std::vector<std::uint8_t> buf(kBlockPayloadBytes, 0);
        std::size_t offset = 0;

        // Bits per uniform cross-sum element: ceil(log2(kS+1)) = bit_width(kS)
        const auto kBitsPerElement = static_cast<std::uint8_t>(
            std::bit_width(static_cast<unsigned>(kS)));

        // 1. Write kS LH digests (kLHDigestBytes each)
        for (std::uint16_t r = 0; r < kS; ++r) {
            std::memcpy(buf.data() + offset, lh_[r].data(), kLHDigestBytes); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            offset += kLHDigestBytes;
        }

        // 2. Write BH digest (32 bytes)
        std::memcpy(buf.data() + offset, bh_.data(), kBHDigestBytes); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        offset += kBHDigestBytes;

        // 3. Write DI byte
        buf[offset] = di_;
        offset += 1;

        // 4-9. Bit-pack cross-sum vectors starting at byte offset
        std::size_t bitOffset = offset * 8;

        // 4. LSM: kS × b bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, lsm_[k], kBitsPerElement); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // 5. VSM: kS × b bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, vsm_[k], kBitsPerElement); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // 6. DSM: kDiagCount variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, dsm_[k], n); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // 7. XSM: kDiagCount variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, xsm_[k], n); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // 8. LTP1SM: b bits per element
        for (std::uint16_t k = 0; k < kS; ++k) {
            const auto n = static_cast<std::uint8_t>(
                std::bit_width(decompress::solvers::ltpLineLen(k)));
            packBits(buf, bitOffset, ltp1sm_[k], n); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        // 9. LTP2SM: b bits per element
        for (std::uint16_t k = 0; k < kS; ++k) {
            const auto n = static_cast<std::uint8_t>(
                std::bit_width(decompress::solvers::ltpLineLen(k)));
            packBits(buf, bitOffset, ltp2sm_[k], n); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }

        return buf;
    }

} // namespace crsce::common::format
