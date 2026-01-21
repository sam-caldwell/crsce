/**
 * @file Decompressor_parse.cpp
 */
#include "decompress/Decompressor/Decompressor.h"
#include "common/Util/Crc32.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace crsce::decompress {
    using crsce::common::util::crc32_ieee;

    namespace {
        template<typename T>
        inline T load_le(std::span<const std::uint8_t> s) {
            T v = 0;
            std::size_t shift = 0;
            for (const auto byte: s) {
                v = static_cast<T>(v | (static_cast<T>(byte) << shift));
                shift += 8U;
            }
            return v;
        }
    } // anonymous namespace

    bool Decompressor::parse_header(const std::array<std::uint8_t, kHeaderSize> &hdr,
                                    HeaderV1Fields &out) {
        // Magic "CRSC"
        const std::array<std::uint8_t, 4> magic{'C', 'R', 'S', 'C'};
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

    bool Decompressor::split_payload(const std::vector<std::uint8_t> &block,
                                     std::span<const std::uint8_t> &out_lh,
                                     std::span<const std::uint8_t> &out_sums) {
        if (block.size() != kBlockBytes) {
            return false;
        }
        const std::span<const std::uint8_t> sp{block};
        out_lh = sp.first(kLhBytes);
        out_sums = sp.subspan(kLhBytes, kSumsBytes);
        return true;
    }
} // namespace crsce::decompress
