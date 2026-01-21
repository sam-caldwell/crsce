/**
 * @file decompressor_parse_header_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Decompressor/Decompressor.h"
#include "common/Format/HeaderV1.h"

using crsce::decompress::Decompressor;
using crsce::decompress::HeaderV1Fields;
using crsce::common::format::HeaderV1;

TEST(Decompressor, ParseHeaderOk) { // NOLINT
    const std::uint64_t orig = 12345U;
    const std::uint64_t blocks = 7U;
    const auto hdr_bytes = HeaderV1::pack(orig, blocks);
    std::array<std::uint8_t, Decompressor::kHeaderSize> hdr{};
    std::ranges::copy(hdr_bytes, hdr.begin());

    HeaderV1Fields out{};
    ASSERT_TRUE(Decompressor::parse_header(hdr, out));
    EXPECT_EQ(out.original_size_bytes, orig);
    EXPECT_EQ(out.block_count, blocks);
}
