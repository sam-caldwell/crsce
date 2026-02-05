/**
 * @file compress_ctor_seed_and_initial_state_test.cpp
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "common/BitHashBuffer/detail/Sha256Types.h"

using crsce::compress::Compress;
using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

namespace {
    std::array<u8, 64> row_first_bit_one_rest_zero_with_pad() {
        std::array<u8, 64> row{};
        row.fill(0);
        row[0] = 0x80; // first bit 1 (MSB)
        return row;
    }
}

/**
 * @name CompressCtor.InitialLhCountZeroAndRowHashIgnoresSeed
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressCtor, InitialLhCountZeroAndRowHashIgnoresSeed) { // NOLINT
    // In v1 per-row LH, seed does not affect digest; check deterministic row hash
    Compress cx("in.bin", "out.crsc");
    EXPECT_EQ(cx.lh_count(), 0U);

    // Push one '1' bit then finalize the block to pad the rest of row0
    cx.push_bit(true);
    cx.finalize_block();

    const auto row = row_first_bit_one_rest_zero_with_pad();
    const auto expected = sha256_digest(row.data(), row.size());

    const auto got = cx.pop_all_lh_bytes();
    ASSERT_EQ(got.size(), 32U);
    for (std::size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(got[i], expected[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
}

/**

 * @name CompressCtor.CustomSeedDoesNotChangeRowHash

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CompressCtor, CustomSeedDoesNotChangeRowHash) { // NOLINT
    // Use a distinct seed value
    const std::string custom_seed = "MY_SEED";
    Compress cx("in.bin", "out.crsc", custom_seed);
    EXPECT_EQ(cx.lh_count(), 0U);

    // Push one '1' bit then finalize the block to pad the rest of row0
    cx.push_bit(true);
    cx.finalize_block();

    const auto row = row_first_bit_one_rest_zero_with_pad();
    const auto expected = sha256_digest(row.data(), row.size());

    const auto got = cx.pop_all_lh_bytes();
    ASSERT_EQ(got.size(), 32U);
    for (std::size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(got[i], expected[i]); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
}
