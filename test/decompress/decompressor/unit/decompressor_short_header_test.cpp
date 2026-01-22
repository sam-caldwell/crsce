/**
 * @file decompressor_short_header_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <array>
#include <fstream>
#include <ios>
#include <string>

#include "decompress/Decompressor/Decompressor.h"

using crsce::decompress::Decompressor;
using crsce::decompress::HeaderV1Fields;

/**

 * @name Decompressor.ShortHeaderReturnsFalse

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(Decompressor, ShortHeaderReturnsFalse) { // NOLINT
    const std::string p = std::string(TEST_BINARY_DIR) + "/d_trunc_hdr.crsc";
    // Write fewer than kHeaderSize bytes
    {
        std::ofstream f(p, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::array<char, 10> junk{};
        f.write(junk.data(), static_cast<std::streamsize>(junk.size()));
    }
    Decompressor dx(p);
    HeaderV1Fields hdr{};
    EXPECT_FALSE(dx.read_header(hdr));
    // With no header, blocks_remaining is zero and read_block should be null
    auto blk = dx.read_block();
    EXPECT_FALSE(blk.has_value());
}
