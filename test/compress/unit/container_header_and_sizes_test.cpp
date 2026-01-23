/**
 * @file container_header_and_sizes_test.cpp
 * @brief Integration: run Compress on tiny inputs; validate header and file size.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

namespace fs = std::filesystem;

namespace {
    inline std::uint16_t rd_le16(const std::array<std::uint8_t, 28> &b, const std::size_t off) {
        return static_cast<std::uint16_t>(b.at(off + 0)) |
               static_cast<std::uint16_t>(b.at(off + 1)) << 8U;
    }

    inline std::uint64_t rd_le64(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8U) | b.at(off + static_cast<std::size_t>(i));
        }
        return v;
    }

    inline std::uint32_t rd_le32(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        return static_cast<std::uint32_t>(b.at(off + 0)) |
               (static_cast<std::uint32_t>(b.at(off + 1)) << 8U) |
               (static_cast<std::uint32_t>(b.at(off + 2)) << 16U) |
               (static_cast<std::uint32_t>(b.at(off + 3)) << 24U);
    }
}

/**

 * @name ContainerHeader.TinyAndEmptyInputs

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(ContainerHeader, TinyAndEmptyInputs) { // NOLINT
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/chdr_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/chdr_out.crsce";

    // Write 2-byte input
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        const std::array<char, 2> b{'A', 'B'};
        f.write(b.data(), 2);
    }
    {
        crsce::compress::Compress cx(in_path, out_path);
        ASSERT_TRUE(cx.compress_file());
    }
    // Read header
    std::array<std::uint8_t, 28> hdr{};
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.read(reinterpret_cast<char *>(hdr.data()), 28); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), 28);
    }
    EXPECT_EQ(hdr[0], 'C');
    EXPECT_EQ(hdr[1], 'R');
    EXPECT_EQ(hdr[2], 'S');
    EXPECT_EQ(hdr[3], 'C');
    EXPECT_EQ(rd_le16(hdr, 4), 1U);
    EXPECT_EQ(rd_le16(hdr, 6), 28U);
    EXPECT_EQ(rd_le64(hdr, 8), 2U);
    EXPECT_EQ(rd_le64(hdr, 16), 1U); // 2 bytes -> 16 bits -> 1 block
    // File size should be 28 + 18,652
    EXPECT_EQ(fs::file_size(out_path), 28U + 18652U);

    // Empty input
    {
        const std::ofstream f(in_path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(f.good());
    }
    {
        crsce::compress::Compress cx(in_path, out_path);
        ASSERT_TRUE(cx.compress_file());
    }
    {
        std::ifstream f(out_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.read(reinterpret_cast<char *>(hdr.data()), 28); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_EQ(f.gcount(), 28);
    }
    EXPECT_EQ(rd_le64(hdr, 8), 0U);
    EXPECT_EQ(rd_le64(hdr, 16), 0U);
    EXPECT_EQ(fs::file_size(out_path), 28U);
}
