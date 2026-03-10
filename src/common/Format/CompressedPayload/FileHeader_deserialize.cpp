/**
 * @file FileHeader_deserialize.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FileHeader::deserialize() implementation.
 */
#include "common/Format/CompressedPayload/FileHeader.h"
#include "common/Util/crc32_ieee.h"

#include <cstdint>
#include <cstring>

#include "common/exceptions/DecompressHeaderInvalid.h"

namespace crsce::common::format {

    /**
     * @name deserialize
     * @brief Deserialize a 28-byte buffer into a FileHeader.
     * @details Validates buffer length, magic number, and CRC-32 checksum.
     *          All multi-byte fields are read as little-endian.
     * @param data Pointer to at least 28 bytes of header data.
     * @param len Length of the buffer (must be >= 28).
     * @return Deserialized FileHeader.
     * @throws DecompressHeaderInvalid if len < 28, magic mismatch, or CRC-32 mismatch.
     */
    FileHeader FileHeader::deserialize(const std::uint8_t *data, const std::size_t len) {
        if (len < kHeaderBytes) {
            throw exceptions::DecompressHeaderInvalid("FileHeader::deserialize: buffer too small (need 28 bytes)");
        }

        // Read and verify magic
        std::uint32_t magic = 0;
        std::memcpy(&magic, data + 0, 4); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (magic != kMagic) {
            throw exceptions::DecompressHeaderInvalid("FileHeader::deserialize: bad magic number");
        }

        // Read and verify CRC-32 over bytes 0-23
        std::uint32_t storedCrc = 0;
        std::memcpy(&storedCrc, data + 24, 4); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const std::uint32_t computedCrc = util::crc32_ieee(data, 24);
        if (storedCrc != computedCrc) {
            throw exceptions::DecompressHeaderInvalid("FileHeader::deserialize: CRC-32 mismatch");
        }

        FileHeader hdr;
        std::memcpy(&hdr.originalFileSizeBytes, data + 8, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::memcpy(&hdr.blockCount, data + 16, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        return hdr;
    }

} // namespace crsce::common::format
