/**
 * @file header_v1_pack_test.cpp
 * @brief Unit: verify HeaderV1::pack packs a correct 28-byte header and CRC.
 */
#include "common/Format/HeaderV1.h"
#include "common/Util/detail/crc32_ieee.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>

using crsce::common::format::HeaderV1;
using crsce::common::util::crc32_ieee;

namespace {
    inline std::uint16_t rd_le16(const std::array<std::uint8_t, HeaderV1::kSize> &b, const std::size_t off) {
        return static_cast<std::uint16_t>(b.at(off + 0)) |
               static_cast<std::uint16_t>(b.at(off + 1)) << 8U;
    }
    inline std::uint32_t rd_le32(const std::array<std::uint8_t, HeaderV1::kSize> &b, const std::size_t off) {
        return static_cast<std::uint32_t>(b.at(off + 0)) |
               (static_cast<std::uint32_t>(b.at(off + 1)) << 8U) |
               (static_cast<std::uint32_t>(b.at(off + 2)) << 16U) |
               (static_cast<std::uint32_t>(b.at(off + 3)) << 24U);
    }
    inline std::uint64_t rd_le64(const std::array<std::uint8_t, HeaderV1::kSize> &b, const std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8U) | b.at(off + static_cast<std::size_t>(i));
        }
        return v;
    }
}

TEST(HeaderV1Pack, Constants) { // NOLINT(readability-identifier-naming)
    EXPECT_EQ(HeaderV1::kSize, 28U);
    EXPECT_EQ(HeaderV1::kVersion, 1U);
}

TEST(HeaderV1Pack, ZeroValues) { // NOLINT(readability-identifier-naming)
    const auto hdr = HeaderV1::pack(0ULL, 0ULL);
    // Magic
    EXPECT_EQ(hdr[0], 'C');
    EXPECT_EQ(hdr[1], 'R');
    EXPECT_EQ(hdr[2], 'S');
    EXPECT_EQ(hdr[3], 'C');
    // Version and size
    EXPECT_EQ(rd_le16(hdr, 4), HeaderV1::kVersion);
    EXPECT_EQ(rd_le16(hdr, 6), HeaderV1::kSize);
    // Sizes
    EXPECT_EQ(rd_le64(hdr, 8), 0ULL);
    EXPECT_EQ(rd_le64(hdr, 16), 0ULL);
    // CRC
    const auto crc_calc = crc32_ieee(hdr.data(), 24U);
    EXPECT_EQ(rd_le32(hdr, 24), crc_calc);
}

TEST(HeaderV1Pack, VariousValuesAndCrc) { // NOLINT(readability-identifier-naming)
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> cases = {
        {1ULL, 0ULL},
        {0ULL, 1ULL},
        {12345ULL, 6789ULL},
        {0x0000000100000000ULL, 0x0000000001000000ULL}, // 2^32 and 2^24
        {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}
    };
    for (const auto &kv : cases) {
        const auto orig = kv.first;
        const auto blocks = kv.second;
        const auto hdr = HeaderV1::pack(orig, blocks);
        // Magic/version/size
        EXPECT_EQ(hdr[0], 'C');
        EXPECT_EQ(hdr[1], 'R');
        EXPECT_EQ(hdr[2], 'S');
        EXPECT_EQ(hdr[3], 'C');
        EXPECT_EQ(rd_le16(hdr, 4), HeaderV1::kVersion);
        EXPECT_EQ(rd_le16(hdr, 6), HeaderV1::kSize);
        // Fields
        EXPECT_EQ(rd_le64(hdr, 8), orig);
        EXPECT_EQ(rd_le64(hdr, 16), blocks);
        // CRC
        const auto crc_calc = crc32_ieee(hdr.data(), 24U);
        EXPECT_EQ(rd_le32(hdr, 24), crc_calc);
    }
}
