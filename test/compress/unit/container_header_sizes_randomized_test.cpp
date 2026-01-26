/**
 * @file container_header_sizes_randomized_test.cpp
 * @brief Verify header fields and file sizes for a range of input lengths.
 */
#include "compress/Compress/Compress.h"
#include "common/Util/detail/crc32_ieee.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using crsce::common::util::crc32_ieee;

namespace {
    inline std::uint16_t rd_le16(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        return static_cast<std::uint16_t>(b.at(off + 0)) |
               static_cast<std::uint16_t>(b.at(off + 1)) << 8U;
    }

    inline std::uint32_t rd_le32(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        return static_cast<std::uint32_t>(b.at(off + 0)) |
               (static_cast<std::uint32_t>(b.at(off + 1)) << 8U) |
               (static_cast<std::uint32_t>(b.at(off + 2)) << 16U) |
               (static_cast<std::uint32_t>(b.at(off + 3)) << 24U);
    }

    inline std::uint64_t rd_le64(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8U) | b.at(off + static_cast<std::size_t>(i));
        }
        return v;
    }
} // namespace

/**
 * @name ContainerHeader.RandomizedLengths
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(ContainerHeader, RandomizedLengths) { // NOLINT
    const std::vector<std::size_t> sizes{
        0, 1, 2, 7, 8, 15, 16, 63, 64, 127, 128, 255, 256, 511, 512, 1023, 1024
    };
    std::mt19937 rng(1337);
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/rand_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/rand_out.crsce";

    for (const auto len: sizes) {
        // Generate deterministic random bytes
        std::vector<std::uint8_t> buf(len);
        for (auto &x: buf) { x = static_cast<std::uint8_t>(rng()); }
        {
            std::ofstream f(in_path, std::ios::binary);
            ASSERT_TRUE(f.good());
            const std::vector<char> cbuf(buf.begin(), buf.end());
            f.write(cbuf.data(), static_cast<std::streamsize>(cbuf.size()));
        }

        crsce::compress::Compress cx(in_path, out_path);
        ASSERT_TRUE(cx.compress_file());

        std::array<std::uint8_t, 28> hdr{};
        {
            std::ifstream f(out_path, std::ios::binary);
            ASSERT_TRUE(f.good());
            f.read(reinterpret_cast<char *>(hdr.data()), 28); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            ASSERT_EQ(f.gcount(), 28);
        }
        // Magic/version/size
        EXPECT_EQ(hdr[0], 'C');
        EXPECT_EQ(hdr[1], 'R');
        EXPECT_EQ(hdr[2], 'S');
        EXPECT_EQ(hdr[3], 'C');
        EXPECT_EQ(rd_le16(hdr, 4), 1U);
        EXPECT_EQ(rd_le16(hdr, 6), 28U);
        EXPECT_EQ(rd_le64(hdr, 8), static_cast<std::uint64_t>(len));

        const auto total_bits = static_cast<std::uint64_t>(len) * 8U;
        const auto bits_per_block = static_cast<std::uint64_t>(511ULL * 511ULL);
        const auto expected_blocks = (total_bits == 0) ? 0ULL : ((total_bits + bits_per_block - 1) / bits_per_block);
        EXPECT_EQ(rd_le64(hdr, 16), expected_blocks);

        // CRC check
        const auto crc_calc = crc32_ieee(hdr.data(), 24U);
        const auto crc_stored = rd_le32(hdr, 24);
        EXPECT_EQ(crc_calc, crc_stored);

        const auto expected_size = 28ULL + (expected_blocks * 18652ULL);
        EXPECT_EQ(fs::file_size(out_path), expected_size);
    }
}
