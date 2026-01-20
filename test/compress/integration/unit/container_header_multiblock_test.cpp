/**
 * @file container_header_multiblock_test.cpp
 * @brief Ensure block_count > 1 and file size equals header + N*18652 for large input.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
    inline std::uint64_t rd_le64(const std::array<std::uint8_t, 28> &b, std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) { v = (v << 8U) | b.at(off + static_cast<std::size_t>(i)); }
        return v;
    }
}

TEST(ContainerHeader, MultiBlockCase) { // NOLINT
    // Bytes needed to exceed one block: ceil(511*511/8) == 32641
    const std::size_t bytes = 32641;
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/mb_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/mb_out.crsce";
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<char> buf(bytes, 'X');
        f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
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
    const auto blocks = rd_le64(hdr, 16);
    EXPECT_EQ(blocks, 2U);
    EXPECT_EQ(fs::file_size(out_path), 28U + (2U * 18652U));
}
