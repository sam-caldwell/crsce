/**
 * @file HeaderV1.cpp
 * @brief Implementation
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include "common/Format/HeaderV1.h"
#include "common/Format/detail/put_le.h"
#include "common/Util/detail/crc32_ieee.h"
#include <array>
#include <cstdint>
#include <cstddef>

namespace crsce::common::format {
    /**
     * @name HeaderV1::pack
     * @brief Pack a v1 header into a 28-byte little-endian buffer.
     * @param original_size_bytes Original uncompressed size in bytes.
     * @param block_count Number of compressed blocks.
     * @return std::array<std::uint8_t, kSize> Packed header bytes.
     */
    std::array<std::uint8_t, HeaderV1::kSize> HeaderV1::pack(const std::uint64_t original_size_bytes,
                                                             const std::uint64_t block_count) {
        std::array<std::uint8_t, kSize> b{};
        // magic "CRSC"
        b[0] = 'C';
        b[1] = 'R';
        b[2] = 'S';
        b[3] = 'C';
        detail::put_le16(b, 4, kVersion);
        detail::put_le16(b, 6, static_cast<std::uint16_t>(kSize));
        detail::put_le64(b, 8, original_size_bytes);
        detail::put_le64(b, 16, block_count);
        // CRC32 over first 24 bytes
        const auto crc = crsce::common::util::crc32_ieee(b.data(), 24U);
        detail::put_le32(b, 24, crc);
        return b;
    }
} // namespace crsce::common::format
