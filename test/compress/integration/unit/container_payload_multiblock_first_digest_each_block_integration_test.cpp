/**
 * @file container_payload_multiblock_first_digest_each_block_integration_test.cpp
 * @brief For a two-block zero input, verify LH[0] of each block matches expected and equals between blocks.
 */
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

TEST(ContainerPayload, MultiBlockFirstDigestPerBlock) { // NOLINT
    namespace fs = std::filesystem;
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/cp_mb_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/cp_mb_out.crsce";

    // 32,641 zero bytes -> ceil(511*511/8) => 2 blocks
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<char> buf(32641, 0);
        f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    }

    crsce::compress::Compress cx(in_path, out_path);
    ASSERT_TRUE(cx.compress_file());

    // Read the first digest (32 bytes) of block 0 and block 1
    std::array<std::uint8_t, 32> d_b0{};
    std::array<std::uint8_t, 32> d_b1{};
    const std::size_t block_bytes = 18652U;
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.seekg(28, std::ios::beg);
        f.read(reinterpret_cast<char *>(d_b0.data()), 32); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), 32);
        f.seekg(28 + static_cast<std::streamoff>(block_bytes), std::ios::beg);
        f.read(reinterpret_cast<char *>(d_b1.data()), 32); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), 32);
    }

    // Expected digest for zero row with seed hash
    const std::string seed = "CRSCE_v1_seed";
    const std::vector<u8> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());
    std::array<u8, 64> zeros{};
    std::array<u8, 96> buf{};
    std::ranges::copy(seed_hash, buf.begin());
    std::ranges::copy(zeros, std::next(buf.begin(), 32));
    const auto expected = sha256_digest(buf.data(), buf.size());

    for (std::size_t i = 0; i < 32; ++i) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(d_b0[i], expected[i]);   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(d_b1[i], expected[i]);   // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }

    // File size sanity: header + 2 blocks
    EXPECT_EQ(fs::file_size(out_path), 28U + (2U * block_bytes));
}
