/**
 * @file CompressedPayload_serializeBlock.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CompressedPayload::serializeBlock() implementation.
 *
 * Serializes one compressed block into exactly kBlockPayloadBytes (15,749) bytes:
 *   1. 511 x 20-byte LH digests (SHA-1)     (10,220 bytes)
 *   2. 32-byte BH digest (SHA-256)           (32 bytes)
 *   3. 1-byte DI                             (1 byte)
 *   4. LSM: 511 x 9 bits, MSB-first packed   (4,599 bits)
 *   5. VSM: 511 x 9 bits, MSB-first packed   (4,599 bits)
 *   6. DSM: 1021 variable-width, MSB-first   (8,185 bits)
 *   7. XSM: 1021 variable-width, MSB-first   (8,185 bits)
 *   8. HSM1: 511 x 9 bits, MSB-first packed  (4,599 bits)
 *   9. SFC1: 511 x 9 bits, MSB-first packed  (4,599 bits)
 *  10. HSM2: 511 x 9 bits, MSB-first packed  (4,599 bits)
 *  11. SFC2: 511 x 9 bits, MSB-first packed  (4,599 bits)
 *   Total cross-sum region: 43,964 bits = 5,496 bytes (partial byte zero-padded)
 *   Grand total: 10,220 + 32 + 1 + 5,496 = 15,749 bytes
 */
#include "common/Format/CompressedPayload/CompressedPayload.h"

#include <cstdint>
#include <cstring>
#include <vector>

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

        // 1. Write 511 LH digests (20 bytes each)
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

        // 4-11. Bit-pack cross-sum vectors starting at byte offset
        std::size_t bitOffset = offset * 8;

        // 4. LSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, lsm_[k], 9);
        }

        // 5. VSM: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, vsm_[k], 9);
        }

        // 6. DSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, dsm_[k], n);
        }

        // 7. XSM: 1021 variable-width
        for (std::uint16_t k = 0; k < kDiagCount; ++k) {
            const auto n = diagBits(k);
            packBits(buf, bitOffset, xsm_[k], n);
        }

        // 8. HSM1: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, hsm1_[k], 9);
        }

        // 9. SFC1: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, sfc1_[k], 9);
        }

        // 10. HSM2: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, hsm2_[k], 9);
        }

        // 11. SFC2: 511 x 9 bits
        for (std::uint16_t k = 0; k < kS; ++k) {
            packBits(buf, bitOffset, sfc2_[k], 9);
        }

        return buf;
    }

} // namespace crsce::common::format
