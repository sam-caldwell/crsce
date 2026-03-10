/**
 * @file FileHeader.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CRSCE file header (28 bytes) value type for serialization and deserialization.
 *
 * The file header encodes the magic number, format version, header size,
 * original file size, block count, and a CRC-32 checksum over bytes 0-23.
 * All multi-byte integers are stored in little-endian byte order.
 */
#pragma once

#include <cstdint>
#include <vector>

namespace crsce::common::format {

    /**
     * @struct FileHeader
     * @name FileHeader
     * @brief 28-byte CRSCE file header with magic, version, sizes, and CRC-32.
     * @details
     * Layout (all multi-byte fields little-endian):
     *   Offset  Size  Type      Field
     *    0       4    char[4]   magic ("CRSC" = 0x43525343)
     *    4       2    uint16    version (1)
     *    6       2    uint16    header_bytes (28)
     *    8       8    uint64    original_file_size_bytes
     *   16       8    uint64    block_count
     *   24       4    uint32    header_crc32 (CRC-32 over bytes 0-23)
     */
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    struct FileHeader {
        /**
         * @name kMagic
         * @brief Magic number identifying a CRSCE file: ASCII "CRSC".
         */
        static constexpr std::uint32_t kMagic = 0x43525343;

        /**
         * @name kVersion
         * @brief Current format version.
         */
        static constexpr std::uint16_t kVersion = 1;

        /**
         * @name kHeaderBytes
         * @brief Total size of the file header in bytes.
         */
        static constexpr std::uint16_t kHeaderBytes = 28;

        /**
         * @name originalFileSizeBytes
         * @brief Size of the original uncompressed file in bytes.
         */
        std::uint64_t originalFileSizeBytes{0};

        /**
         * @name blockCount
         * @brief Number of compressed blocks in the file.
         */
        std::uint64_t blockCount{0};

        /**
         * @name serialize
         * @brief Serialize the header to a 28-byte little-endian buffer.
         * @return 28-byte vector containing the serialized header with CRC-32.
         * @throws None
         */
        [[nodiscard]] std::vector<std::uint8_t> serialize() const;

        /**
         * @name deserialize
         * @brief Deserialize a 28-byte buffer into a FileHeader.
         * @param data Pointer to at least 28 bytes of header data.
         * @param len Length of the buffer (must be >= 28).
         * @return Deserialized FileHeader.
         * @throws DecompressHeaderInvalid if len < 28, magic mismatch, or CRC-32 mismatch.
         */
        static FileHeader deserialize(const std::uint8_t *data, std::size_t len);
    };
    // NOLINTEND(misc-non-private-member-variables-in-classes)

} // namespace crsce::common::format
