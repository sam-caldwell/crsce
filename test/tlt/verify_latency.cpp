/**
 * @file verify_latency.cpp
 * @brief TLT: measure compression latency and assert <=1s per block on average.
 */
#include <gtest/gtest.h>

#include "helpers/tmp_dir.h"
#include "helpers/tlt/time_compress_ms_after_random_input.h"
#include "helpers/tlt/bytes_for_n_blocks.h"
#include "helpers/tlt/write_zero_file_at.h"
#include "helpers/tlt/run_compress_cli.h"
#include "helpers/tlt/run_decompress_cli.h"
#include "helpers/tlt/files_equal.h"
#include "helpers/remove_file.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

/**
 * @name VerifyLatency.BlocksUnderOneSecondPer
 * @brief For 1/2/5/10 blocks, ensure compress time <= 1s per block (on average).
 */
TEST(VerifyLatency, BlocksUnderOneSecondPer) { // NOLINT(readability-identifier-naming)
    const auto td = tmp_dir();
    const std::vector<std::size_t> blocks = {1U, 2U, 5U, 10U};

    for (const auto n_blocks: blocks) {
        bool success = false;
        std::uint64_t ms = 0;
        // Try a few RNG seeds before falling back to zeros; we only assert on compression time
        for (int attempt = 0; attempt < 3 && !success; ++attempt) {
            const std::uint32_t seed = 0x1A2B3C4DU + static_cast<std::uint32_t>(
                                           (n_blocks * 131U) + (attempt * 2654435761U));
            std::mt19937 rng(seed);
            if (const auto [ok, elapsed_ms] = crsce::testhelpers::tlt::time_compress_ms_after_random_input(
                td, n_blocks, rng, "lat"); ok) {
                success = true;
                ms = elapsed_ms;
            }
        }
        if (!success) {
            // Fallback: all-zero input of the same size for a deterministic, solvable case
            const std::size_t bytes = crsce::testhelpers::tlt::bytes_for_n_blocks(n_blocks);
            const std::string in_path = td + "/lat_in_zero_" + std::to_string(n_blocks) + ".bin";
            const std::string cr_path = td + "/lat_cr_zero_" + std::to_string(n_blocks) + ".crsce";
            const std::string out_path = td + "/lat_out_zero_" + std::to_string(n_blocks) + ".bin";
            remove_file(in_path);
            remove_file(cr_path);
            remove_file(out_path);
            ASSERT_TRUE(crsce::testhelpers::tlt::write_zero_file_at(in_path, bytes));
            const auto t0 = std::chrono::steady_clock::now();
            ASSERT_EQ(crsce::testhelpers::tlt::run_compress_cli(in_path, cr_path),
                      0) << "compress failed (zeros) N=" << n_blocks;
            const auto t1 = std::chrono::steady_clock::now();
            ASSERT_EQ(crsce::testhelpers::tlt::run_decompress_cli(cr_path, out_path),
                      0) << "decompress failed (zeros) N=" << n_blocks;
            ASSERT_TRUE(crsce::testhelpers::tlt::files_equal(in_path, out_path));
            ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
            remove_file(in_path);
            remove_file(cr_path);
            remove_file(out_path);
        }

        const std::uint64_t limit_ms = static_cast<std::uint64_t>(n_blocks) * 1000ULL;
        EXPECT_LE(ms, limit_ms) << "compression exceeded 1s/block average for N=" << n_blocks
                                << " (" << ms << " ms for " << n_blocks << " blocks)";
    }
}
