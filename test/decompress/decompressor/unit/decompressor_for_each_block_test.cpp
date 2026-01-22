/**
 * @file decompressor_for_each_block_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

#include "compress/Compress/Compress.h"
#include "decompress/Decompressor/Decompressor.h"

using crsce::compress::Compress;
using crsce::decompress::Decompressor;
using crsce::decompress::HeaderV1Fields;

/**

 * @name DecompressorDrive.ForEachBlockHappyTwoBlocks

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(DecompressorDrive, ForEachBlockHappyTwoBlocks) { // NOLINT
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/d_mb_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/d_mb_out.crsc";
    // Create 32641 bytes -> 2 blocks
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<char> data(32641, 0);
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    {
        Compress cx(in_path, out_path);
        ASSERT_TRUE(cx.compress_file());
    }
    Decompressor dx(out_path);
    HeaderV1Fields hdr{};
    std::size_t calls = 0;
    const bool ok = dx.for_each_block(hdr, [&](std::span<const std::uint8_t> lh, std::span<const std::uint8_t> sums) {
        ++calls;
        ASSERT_EQ(lh.size(), Decompressor::kLhBytes);
        ASSERT_EQ(sums.size(), Decompressor::kSumsBytes);
    });
    ASSERT_TRUE(ok);
    EXPECT_EQ(hdr.block_count, 2U);
    EXPECT_EQ(calls, 2U);
}

/**

 * @name DecompressorDrive.ForEachBlockFailsOnBadHeader

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(DecompressorDrive, ForEachBlockFailsOnBadHeader) { // NOLINT
    const std::string bad_path = std::string(TEST_BINARY_DIR) + "/d_bad_header.crsc";
    // Write 28 bytes of junk header
    {
        std::ofstream f(bad_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<char> junk(28, 'X');
        f.write(junk.data(), static_cast<std::streamsize>(junk.size()));
    }
    Decompressor dx(bad_path);
    HeaderV1Fields hdr{};
    std::size_t calls = 0;
    const bool ok = dx.for_each_block(hdr, [&](std::span<const std::uint8_t>, std::span<const std::uint8_t>) {
        ++calls;
    });
    EXPECT_FALSE(ok);
    EXPECT_EQ(calls, 0U);
}

/**

 * @name DecompressorDrive.ForEachBlockStopsOnTruncatedPayload

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(DecompressorDrive, ForEachBlockStopsOnTruncatedPayload) { // NOLINT
    const std::string in = std::string(TEST_BINARY_DIR) + "/d_trunc2_in.bin";
    const std::string out = std::string(TEST_BINARY_DIR) + "/d_trunc2_out.crsc";
    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.put(static_cast<char>(0x00));
    }
    {
        Compress cx(in, out);
        ASSERT_TRUE(cx.compress_file());
    }
    // Truncate to header + 100 bytes (short of one payload)
    const std::string trunc = std::string(TEST_BINARY_DIR) + "/d_trunc2_out_short.crsc";
    {
        std::ifstream in_f(out, std::ios::binary);
        std::ofstream out_f(trunc, std::ios::binary);
        ASSERT_TRUE(in_f.good());
        ASSERT_TRUE(out_f.good());
        std::vector<char> buf(Decompressor::kHeaderSize + 100U);
        in_f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        out_f.write(buf.data(), in_f.gcount());
    }
    Decompressor dx(trunc);
    HeaderV1Fields hdr{};
    std::size_t calls = 0;
    const bool ok = dx.for_each_block(hdr, [&](std::span<const std::uint8_t>, std::span<const std::uint8_t>) {
        ++calls;
    });
    EXPECT_FALSE(ok);
    EXPECT_EQ(calls, 0U);
}
