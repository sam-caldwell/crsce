/**
 * @file FileHeader_serialize.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief FileHeader::serialize() implementation.
 */
#include "common/Format/CompressedPayload/FileHeader.h"
#include "common/Util/crc32_ieee.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace crsce::common::format {

    /**
     * @name serialize
     * @brief Serialize the header to a 28-byte little-endian buffer with CRC-32.
     * @details Writes magic, version, header size, original file size, block count,
     *          then computes CRC-32 over bytes 0-23 and appends it at offset 24.
     * @return 28-byte vector containing the serialized header.
     * @throws None
     */
    std::vector<std::uint8_t> FileHeader::serialize() const {
        std::vector<std::uint8_t> buf(kHeaderBytes, 0);

        // Offset 0: magic (little-endian uint32)
        const std::uint32_t magic = kMagic;
        std::memcpy(buf.data() + 0, &magic, 4); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Offset 4: version (little-endian uint16)
        const std::uint16_t version = kVersion;
        std::memcpy(buf.data() + 4, &version, 2); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Offset 6: header_bytes (little-endian uint16)
        const std::uint16_t headerBytes = kHeaderBytes;
        std::memcpy(buf.data() + 6, &headerBytes, 2); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Offset 8: original_file_size_bytes (little-endian uint64)
        std::memcpy(buf.data() + 8, &originalFileSizeBytes, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Offset 16: block_count (little-endian uint64)
        std::memcpy(buf.data() + 16, &blockCount, 8); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        // Offset 24: CRC-32 over bytes 0-23
        const std::uint32_t crc = util::crc32_ieee(buf.data(), 24);
        std::memcpy(buf.data() + 24, &crc, 4); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        return buf;
    }

} // namespace crsce::common::format
