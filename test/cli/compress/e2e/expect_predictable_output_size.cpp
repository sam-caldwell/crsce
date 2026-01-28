/**
 * @file expect_predictable_output_size.cpp
 * @brief E2E: compress CLI produces predictable per‑block output size and total container size.
 */
#include "compress/Cli/detail/run.h"
#include "compress/Compress/Compress.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
    // Decode little-endian 64-bit from a 28-byte header buffer at offset off
    inline std::uint64_t rd_le64(const std::array<std::uint8_t, 28> &b, const std::size_t off) {
        std::uint64_t v = 0;
        for (int i = 7; i >= 0; --i) { v = (v << 8U) | b.at(off + static_cast<std::size_t>(i)); }
        return v;
    }

    // Compute minimal input byte count that yields exactly N blocks
    inline std::size_t bytes_for_n_blocks(const std::size_t n) {
        if (n == 0) { return 0; }
        const auto bits_per_block = static_cast<std::uint64_t>(crsce::compress::Compress::kBitsPerBlock);
        const auto bits_needed = (static_cast<std::uint64_t>(n - 1) * bits_per_block) + 1ULL;
        return static_cast<std::size_t>((bits_needed + 7ULL) / 8ULL);
    }
}

/**
 * @name CompressCLI.ExpectPredictableOutputSize
 * @brief For 1/2/5/10/20 blocks, verify per‑block payload bytes (Cr) and total file size.
 */
TEST(CompressCLI, ExpectPredictableOutputSize) {
    const auto td = tmp_dir();
    constexpr std::size_t kHeaderBytes = 28U;
    constexpr std::size_t kBlockPayloadBytes = 18652U; // 511*32 (LH) + 4*575 (cross-sums)

    // Test the requested block counts
    const std::array<std::size_t, 5> blocks = {1U, 2U, 5U, 10U, 20U};

    // Deterministic RNG for repeatability
    std::mt19937 rng(0xC0FFEEU);
    std::uniform_int_distribution<int> dist(0, 255);

    for (const auto n_blocks : blocks) {
        const auto in = td + "/predict_size_in_" + std::to_string(n_blocks) + ".bin";
        const auto out = td + "/predict_size_out_" + std::to_string(n_blocks) + ".crsce";
        remove_file(in);
        remove_file(out);

        // Generate a random input sized to produce exactly n_blocks in the container
        const std::size_t in_bytes = bytes_for_n_blocks(n_blocks);
        {
            std::ofstream f(in, std::ios::binary);
            ASSERT_TRUE(f.good());
            std::vector<char> buf(in_bytes, 0);
            for (auto &c : buf) { c = static_cast<char>(dist(rng)); }
            f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        }

        // Run CLI: compress -in <in> -out <out>
        std::vector<std::string> av = {"compress", "-in", in, "-out", out};
        std::vector<char *> argv;
        argv.reserve(av.size());
        for (auto &s: av) { argv.push_back(s.data()); }
        const int rc = crsce::compress::cli::run(std::span<char *>{argv.data(), argv.size()});
        ASSERT_EQ(rc, 0);
        ASSERT_TRUE(fs::exists(out));

        // Verify total file size: header + N * block_payload_bytes
        const auto sz = fs::file_size(out);
        const auto expected_total = static_cast<std::uint64_t>(kHeaderBytes) +
                                    (static_cast<std::uint64_t>(n_blocks) * static_cast<std::uint64_t>(kBlockPayloadBytes));
        EXPECT_EQ(sz, expected_total) << "for N=" << n_blocks;

        // Verify per-block payload bytes (Cr): (total - header)/N == kBlockPayloadBytes
        ASSERT_GE(sz, kHeaderBytes);
        const auto payload = sz - kHeaderBytes;
        EXPECT_EQ(payload % n_blocks, 0U) << "payload not divisible by N blocks";
        EXPECT_EQ(payload / n_blocks, kBlockPayloadBytes) << "per-block payload mismatch for N=" << n_blocks;

        // Sanity: header’s block_count matches N
        std::array<std::uint8_t, kHeaderBytes> hdr{};
        {
            std::ifstream f(out, std::ios::binary);
            ASSERT_TRUE(f.good());
            f.read(reinterpret_cast<char *>(hdr.data()), static_cast<std::streamsize>(hdr.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            ASSERT_EQ(f.gcount(), static_cast<std::streamsize>(hdr.size()));
        }
        const auto hdr_blocks = rd_le64(hdr, 16);
        EXPECT_EQ(hdr_blocks, n_blocks);

        remove_file(in);
        remove_file(out);
    }
}
