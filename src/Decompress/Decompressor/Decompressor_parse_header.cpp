/**
 * @file Decompressor_parse_header.cpp
 * @brief Implementation of Decompressor::parse_header.
 * © Sam Caldwell. See LICENSE.txt for details.
 */
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"
#include "decompress/Decompressor/detail/load_le.h"
#include "common/Util/detail/crc32_ieee.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace crsce::decompress {
    using crsce::common::util::crc32_ieee;
    using crsce::decompress::detail::load_le;

    /**
     * @name Decompressor::parse_header
     * @brief Parse header v1 (validates magic, version, sizes, CRC32) into fields.
     * @param hdr 28-byte header buffer.
     * @param out Parsed header fields on success.
     * @return bool True on success; false if invalid.
     */
    bool Decompressor::parse_header(const std::array<std::uint8_t, kHeaderSize> &hdr,
                                    HeaderV1Fields &out) {
        // Magic "CRSC"
        constexpr std::array<std::uint8_t, 4> magic{'C', 'R', 'S', 'C'};
        const auto head_span = std::span<const std::uint8_t>(hdr);
        if (!std::equal(head_span.first<4>().begin(), head_span.first<4>().end(), magic.begin())) {
            return false;
        }
        const auto version = load_le<std::uint16_t>(head_span.subspan(4, 2));
        const auto header_bytes = load_le<std::uint16_t>(head_span.subspan(6, 2));
        if (version != 1U || header_bytes != kHeaderSize) {
            return false;
        }
        out.original_size_bytes = load_le<std::uint64_t>(head_span.subspan(8, 8));
        out.block_count = load_le<std::uint64_t>(head_span.subspan(16, 8));

        // CRC32 over first 24 bytes
        const auto crc = crc32_ieee(hdr.data(), 24U);
        const auto crc_field = load_le<std::uint32_t>(head_span.subspan(24, 4));
        return crc == crc_field;
    }
} // namespace crsce::decompress
