/**
 * @file decompressor_parse_header_bad_cases_test.cpp
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"
#include "common/Format/HeaderV1.h"

using crsce::decompress::Decompressor;
using crsce::decompress::HeaderV1Fields;
using crsce::common::format::HeaderV1;

namespace {
    // ReSharper disable once CppDFAConstantParameter
    // NOLINT BEGIN(CppDFAConstantParameter)
    std::array<std::uint8_t, Decompressor::kHeaderSize> mk_hdr(const std::uint64_t orig, const std::uint64_t blocks) {
    // NOLINT END(CppDFAConstantParameter)
        const auto v = HeaderV1::pack(orig, blocks);
        std::array<std::uint8_t, Decompressor::kHeaderSize> a{};
        std::ranges::copy(v, a.begin());
        return a;
    }
} // namespace

/**
 * @name Decompressor.ParseHeaderBadMagicVersionCrc
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(Decompressor, ParseHeaderBadMagicVersionCrc) { // NOLINT
    HeaderV1Fields out{};
    {
        auto h = mk_hdr(1U, 0U);
        h[0] = 'X';
        EXPECT_FALSE(Decompressor::parse_header(h, out));
    }
    {
        auto h = mk_hdr(1U, 0U);
        // Corrupt version at offset 4 (LE)
        h[4] = 2U;
        h[5] = 0U;
        EXPECT_FALSE(Decompressor::parse_header(h, out));
    }
    {
        auto h = mk_hdr(1U, 0U);
        // Corrupt header size at offset 6
        h[6] = 0x10U;
        h[7] = 0x00U;
        EXPECT_FALSE(Decompressor::parse_header(h, out));
    }
    {
        auto h = mk_hdr(1U, 0U);
        // Corrupt CRC at offset 24 (flip bits)
        h[24] ^= 0xFFU;
        EXPECT_FALSE(Decompressor::parse_header(h, out));
    }
}
