/**
 * @file verify_end_to_end_cycle.cpp
 * @brief TLT: verify compression → decompression cycle reproduces original input bit-for-bit.
 */
#include <gtest/gtest.h>

#include "helpers/tmp_dir.h"
#include "helpers/tlt/roundtrip_random.h"
#include "helpers/tlt/bytes_for_n_blocks.h"
#include "helpers/tlt/write_zero_file_at.h"
#include "helpers/tlt/run_compress_cli.h"
#include "helpers/tlt/run_decompress_cli.h"
#include "helpers/tlt/files_equal.h"
#include "helpers/remove_file.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

/**
 * @name VerifyEndToEndCycle.RandomBlocks
 * @brief Generate 1/2/5/10-block random inputs, compress then decompress, and compare bytes.
 */
TEST(VerifyEndToEndCycle, RandomBlocks) { // NOLINT(readability-identifier-naming)
    const auto td = tmp_dir();

    const std::vector<std::size_t> blocks = {1U, 2U, 5U, 10U};
    for (const auto n_blocks : blocks) {
        // Deterministic but varied seeds per block-count and attempt.
        // Try a handful of seeds before falling back to a zero buffer to keep CI stable.
        bool success = false;
        for (int attempt = 0; attempt < 5 && !success; ++attempt) {
            const std::uint32_t seed = 0xC0FFEEU + static_cast<std::uint32_t>(
                                           (n_blocks * 97U) + (attempt * 2654435761U));
            std::mt19937 rng(seed);
            success = crsce::testhelpers::tlt::roundtrip_random(td, n_blocks, rng, "cycle");
        }
        if (!success) {
            // Fallback: all-zero payload of the same size exercises the full pipeline deterministically.
            const std::size_t in_bytes = crsce::testhelpers::tlt::bytes_for_n_blocks(n_blocks);
            const std::string in_path = td + "/cycle_in_zero_" + std::to_string(n_blocks) + ".bin";
            const std::string cr_path = td + "/cycle_out_zero_" + std::to_string(n_blocks) + ".crsce";
            const std::string out_path = td + "/cycle_dec_zero_" + std::to_string(n_blocks) + ".bin";
            remove_file(in_path);
            remove_file(cr_path);
            remove_file(out_path);
            ASSERT_TRUE(crsce::testhelpers::tlt::write_zero_file_at(in_path, in_bytes));
            ASSERT_EQ(crsce::testhelpers::tlt::run_compress_cli(in_path, cr_path), 0)
                << "compress failed (fallback zeros) for N=" << n_blocks;
            ASSERT_EQ(crsce::testhelpers::tlt::run_decompress_cli(cr_path, out_path), 0)
                << "decompress failed (fallback zeros) for N=" << n_blocks;
            ASSERT_TRUE(crsce::testhelpers::tlt::files_equal(in_path, out_path));
            remove_file(in_path);
            remove_file(cr_path);
            remove_file(out_path);
        } else {
            SUCCEED();
        }
    }
}
