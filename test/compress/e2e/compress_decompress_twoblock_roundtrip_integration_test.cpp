/**
 * @file compress_decompress_twoblock_roundtrip_integration_test.cpp
 * @brief End-to-end API roundtrip: Compress then Decompress a >=2-block zero file.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ios>
#include <span>
#include <string>
#include <vector>

#include "compress/Compress/Compress.h"
#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Decompressor/HeaderV1Fields.h"
#include "decompress/Csm/detail/Csm.h"
#include "helpers/decompress_utils.h"

using crsce::compress::Compress;
using crsce::decompress::Decompressor;
using crsce::decompress::Csm;
using crsce::testhelpers::append_bits_from_csm;
using crsce::testhelpers::solve_block;

namespace {
} // namespace

/**
 * @name CompressDecompressIntegration.ZeroTwoBlockRoundtrip
 * @brief Intent: Ensure Compress API output round trips via Decompressor to original bytes for a >=2-block all-zero
 *        file. Passing indicates the holistic compression/decompression stack reconstructs the original data.
 *        Assumptions: default seed and format v1.
 */
TEST(CompressDecompressIntegration, ZeroTwoBlockRoundtrip) {
    namespace fs = std::filesystem;
    const auto repo_build_dir = std::string(TEST_BINARY_DIR);
    const std::string in = repo_build_dir + "/twoblk_api_src.bin";
    const std::string out = repo_build_dir + "/twoblk_api.crsc";

    // Create >=2-block zero file (40,000 bytes > 32,641 threshold)
    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        std::vector<std::uint8_t> zeros(40000, 0);
        f.write(reinterpret_cast<const char *>(zeros.data()), static_cast<std::streamsize>(zeros.size())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        ASSERT_TRUE(f.good());
    }

    // Compress via API
    {
        Compress cx(in, out);
        ASSERT_TRUE(cx.compress_file());
    }
    ASSERT_TRUE(fs::exists(out));

    // Decompress via API, reconstruct original bits using solver stack
    Decompressor dx(out);
    crsce::decompress::HeaderV1Fields hdr{};

    std::vector<std::uint8_t> rec;
    rec.reserve(40000);
    std::uint8_t curr = 0;
    int bit_pos = 0;
    const std::string seed = "CRSCE_v1_seed";
    const bool ok = dx.for_each_block(hdr, [&](std::span<const std::uint8_t> lh,
                                               std::span<const std::uint8_t> sums) {
        const std::uint64_t total_bits = hdr.original_size_bytes * 8U;
        const std::uint64_t done_bits = (static_cast<std::uint64_t>(rec.size()) * 8U) + static_cast<std::uint64_t>(
                                            bit_pos);
        if (done_bits >= total_bits) { return; } // ignore extras

        Csm csm;
        if (!solve_block(lh, sums, csm, seed)) {
            // Force failure by leaving 'ok' unchanged; outer check will fail on mismatch
            return;
        }
        constexpr std::uint64_t bits_per_block =
                static_cast<std::uint64_t>(Csm::kS) * static_cast<std::uint64_t>(Csm::kS);
        const std::uint64_t remain = total_bits - done_bits;
        const std::uint64_t take = std::min(remain, bits_per_block);
        append_bits_from_csm(csm, take, rec, curr, bit_pos);
    });
    ASSERT_TRUE(ok);
    ASSERT_GE(hdr.block_count, 2U);
    ASSERT_EQ(rec.size(), static_cast<std::size_t>(hdr.original_size_bytes));

    // Load original and compare byte-for-byte
    std::vector<std::uint8_t> orig;
    {
        std::ifstream f(in, std::ios::binary); // NOLINT(misc-const-correctness)
        ASSERT_TRUE(f.good());
        orig.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    ASSERT_EQ(orig.size(), rec.size());
    EXPECT_EQ(orig, rec);
}
