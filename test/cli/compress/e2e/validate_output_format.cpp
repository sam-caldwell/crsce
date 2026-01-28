/**
 * @file validate_output_format.cpp
 * @brief E2E: invoke compress CLI binary and validate CRSCE container format for 0/1/5 blocks.
 */
#include <gtest/gtest.h>

#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"
#include "common/Util/detail/crc32_ieee.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using crsce::common::util::crc32_ieee;

namespace {
    constexpr std::size_t kHeaderBytes = 28U;
    constexpr std::size_t kLhBytesPerBlock = 511U * 32U;    // 16352 bytes
    constexpr std::size_t kCrossSumBytesPerBlock = 2300U;    // 4 * ceil(511*9/8) = 4*575
    constexpr std::size_t kBlockPayload = kLhBytesPerBlock + kCrossSumBytesPerBlock; // 18652 bytes

    inline std::uint16_t rd_le16(const std::array<std::uint8_t, kHeaderBytes> &b, const std::size_t off) {
        return static_cast<std::uint16_t>(b.at(off + 0)) |
               static_cast<std::uint16_t>(b.at(off + 1)) << 8U;
    }

    inline std::uint32_t rd_le32(const std::array<std::uint8_t, kHeaderBytes> &b, const std::size_t off) {
        return static_cast<std::uint32_t>(b.at(off + 0)) |
               (static_cast<std::uint32_t>(b.at(off + 1)) << 8U) |
               (static_cast<std::uint32_t>(b.at(off + 2)) << 16U) |
               (static_cast<std::uint32_t>(b.at(off + 3)) << 24U);
    }

    inline std::uint64_t rd_le64(const std::array<std::uint8_t, kHeaderBytes> &b, const std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) { v = (v << 8U) | b.at(off + static_cast<std::size_t>(i)); }
        return v;
    }

    // Minimal bytes that force exactly n blocks
    inline std::size_t bytes_for_n_blocks(const std::size_t n) {
        if (n == 0) {
            return 0;
        }
        constexpr std::uint64_t kBitsPerBlock = 511ULL * 511ULL;
        const auto bits_needed = (static_cast<std::uint64_t>(n - 1) * kBitsPerBlock) + 1ULL;
        return static_cast<std::size_t>((bits_needed + 7ULL) / 8ULL);
    }

    inline int run_compress_bin(const std::string &in, const std::string &out) {
        // Compress binary built from cmd/compress/main.cpp is placed in TEST_BINARY_DIR
        const std::string exe = std::string(TEST_BINARY_DIR) + "/compress";
        const std::string cmd = exe + " -in " + in + " -out " + out;
        return std::system(cmd.c_str()); // NOLINT(concurrency-mt-unsafe)
    }
}

/**
 * @name CompressCLI.ValidateOutputFormat
 * @brief Generate random inputs that produce 0/1/5 blocks; run CLI binary and validate container.
 */
TEST(CompressCLI, ValidateOutputFormat) {
    const auto td = tmp_dir();
    std::mt19937 rng(0xC0FFEEU);
    std::uniform_int_distribution<int> dist(0, 255);

    const std::vector<std::size_t> blocks = {0U, 1U, 5U};
    for (const auto n_blocks : blocks) {
        const std::string in = td + "/fmt_in_" + std::to_string(n_blocks) + ".bin";
        const std::string out = td + "/fmt_out_" + std::to_string(n_blocks) + ".crsce";
        remove_file(in);
        remove_file(out);

        // Write deterministic random input of appropriate size
        const std::size_t in_bytes = bytes_for_n_blocks(n_blocks);
        {
            std::ofstream f(in, std::ios::binary);
            ASSERT_TRUE(f.good()) << "failed to create input: " << in;
            std::vector<char> buf(in_bytes);
            for (auto &c : buf) {
                c = static_cast<char>(dist(rng));
            }
            if (!buf.empty()) {
                f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
            }
        }

        // Run binary
        const int rc = run_compress_bin(in, out);
        ASSERT_EQ(rc, 0) << "compress binary failed for N=" << n_blocks;
        ASSERT_TRUE(fs::exists(out));

        // Read header
        std::array<std::uint8_t, kHeaderBytes> hdr{};
        {
            std::ifstream f(out, std::ios::binary);
            ASSERT_TRUE(f.good());
            f.read(reinterpret_cast<char *>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(hdr.size()));
        }
        // Magic/version/header-size
        EXPECT_EQ(hdr[0], 'C');
        EXPECT_EQ(hdr[1], 'R');
        EXPECT_EQ(hdr[2], 'S');
        EXPECT_EQ(hdr[3], 'C');
        EXPECT_EQ(rd_le16(hdr, 4), 1U);
        EXPECT_EQ(rd_le16(hdr, 6), kHeaderBytes);

        // Original size matches input length
        const auto orig_sz = rd_le64(hdr, 8);
        EXPECT_EQ(orig_sz, static_cast<std::uint64_t>(in_bytes));

        // Block count expected according to spec
        constexpr std::uint64_t bits_per_block = 511ULL * 511ULL;
        const auto total_bits = static_cast<std::uint64_t>(in_bytes) * 8ULL;
        const auto expected_blocks = (total_bits == 0)
                                     ? 0ULL
                                     : ((total_bits + bits_per_block - 1) / bits_per_block);
        const auto hdr_blocks = rd_le64(hdr, 16);
        EXPECT_EQ(hdr_blocks, expected_blocks);
        EXPECT_EQ(hdr_blocks, static_cast<std::uint64_t>(n_blocks));

        // Header CRC32
        const auto crc_calc = crc32_ieee(hdr.data(), 24U);
        const auto crc_stored = rd_le32(hdr, 24);
        EXPECT_EQ(crc_calc, crc_stored);

        // File size: header + N * payload
        const auto file_sz = fs::file_size(out);
        const auto expected_sz = static_cast<std::uint64_t>(kHeaderBytes) + (hdr_blocks * kBlockPayload);
        EXPECT_EQ(file_sz, expected_sz);

        // For each block, validate payload layout: LH bytes then cross-sums bytes
        if (hdr_blocks > 0) {
            std::ifstream f(out, std::ios::binary);
            ASSERT_TRUE(f.good());
            // Skip header
            f.seekg(static_cast<std::streamoff>(kHeaderBytes), std::ios::beg);
            for (std::uint64_t b = 0; b < hdr_blocks; ++b) {
                std::vector<char> lh(kLhBytesPerBlock);
                f.read(lh.data(), static_cast<std::streamsize>(lh.size()));
                ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(lh.size()))
                    << "LH payload truncated at block " << b;
                std::vector<char> sums(kCrossSumBytesPerBlock);
                f.read(sums.data(), static_cast<std::streamsize>(sums.size()));
                ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(sums.size()))
                    << "cross-sums payload truncated at block " << b;
            }
            // Confirm EOF exactly at expected boundary
            const auto pos = static_cast<std::uint64_t>(f.tellg());
            EXPECT_EQ(pos, expected_sz);
        }

        remove_file(in);
        remove_file(out);
    }
}
