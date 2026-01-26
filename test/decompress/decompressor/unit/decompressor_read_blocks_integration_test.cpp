/**
 * @file decompressor_read_blocks_integration_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <optional>
#include <string>
#include <vector>

#include "compress/Compress/Compress.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"

using crsce::compress::Compress;
using crsce::decompress::Decompressor;
using crsce::decompress::HeaderV1Fields;

/**
 * @name DecompressorIntegration.ReadOneBlockFromCompressedFile
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DecompressorIntegration, ReadOneBlockFromCompressedFile) { // NOLINT
    namespace fs = std::filesystem;
    const std::string in = std::string(TEST_BINARY_DIR) + "/d_oneblock_in.bin";
    const std::string out = std::string(TEST_BINARY_DIR) + "/d_oneblock_out.crsc";
    // Create a small input file (e.g., 1 byte)
    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.put(static_cast<char>(0xF0));
    }
    // Compress it
    {
        Compress cx(in, out);
        ASSERT_TRUE(cx.compress_file());
    }
    ASSERT_TRUE(fs::exists(out));

    // Decompressor: read header and one block
    Decompressor dx(out);
    HeaderV1Fields hdr{};
    ASSERT_TRUE(dx.read_header(hdr));
    EXPECT_EQ(hdr.block_count, 1U);
    auto block = dx.read_block();
    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->size(), Decompressor::kBlockBytes);

    // No more blocks
    auto none = dx.read_block();
    EXPECT_FALSE(none.has_value());

    // Split payload sanity
    std::span<const std::uint8_t> lh;
    std::span<const std::uint8_t> sums;
    ASSERT_TRUE(Decompressor::split_payload(*block, lh, sums));
    EXPECT_EQ(lh.size(), Decompressor::kLhBytes);
    EXPECT_EQ(sums.size(), Decompressor::kSumsBytes);
}

/**
 * @name DecompressorIntegration.TruncatedBlockReturnsNullopt
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(DecompressorIntegration, TruncatedBlockReturnsNullopt) { // NOLINT
    namespace fs = std::filesystem;
    const std::string in = std::string(TEST_BINARY_DIR) + "/d_trunc_in.bin";
    const std::string out = std::string(TEST_BINARY_DIR) + "/d_trunc_out.crsc";
    // Create small input and compress
    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.put(static_cast<char>(0x00));
    }
    {
        Compress cx(in, out);
        ASSERT_TRUE(cx.compress_file());
    }
    // Truncate the file to header + fragment of block
    ASSERT_TRUE(fs::exists(out));
    const auto orig_size = fs::file_size(out);
    ASSERT_GT(orig_size, Decompressor::kHeaderSize);
    const auto truncated = std::string(TEST_BINARY_DIR) + "/d_trunc_out_short.crsc";
    {
        std::ifstream in_f(out, std::ios::binary);
        std::ofstream out_f(truncated, std::ios::binary);
        ASSERT_TRUE(in_f.good());
        ASSERT_TRUE(out_f.good());
        std::vector<char> buf(Decompressor::kHeaderSize + 100U);
        in_f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        out_f.write(buf.data(), in_f.gcount());
    }
    // Decompressor on a truncated file: header ok, first block read fails
    Decompressor dx(truncated);
    HeaderV1Fields hdr{};
    ASSERT_TRUE(dx.read_header(hdr));
    EXPECT_EQ(hdr.block_count, 1U);
    auto block = dx.read_block();
    EXPECT_FALSE(block.has_value());
}
