/**
 * @file compress_lh_counts_two_rows_test.cpp
 * @brief Verify LH digest count and LSM counts for two rows.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>

using crsce::compress::Compress;

/**

 * @name CompressTest.TwoRowsDigestCountAndLsm

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CompressTest, TwoRowsDigestCountAndLsm) {
    Compress c{"in.bin", "out.bin"};

    // Row 0: ones at columns {0,2,4}; rest zeros
    for (std::size_t col = 0; col < Compress::kS; ++col) {
        const bool one = (col == 0) || (col == 2) || (col == 4);
        c.push_bit(one);
    }
    // Row 1: ones at columns {1,3,5}; rest zeros
    for (std::size_t col = 0; col < Compress::kS; ++col) {
        const bool one = (col == 1) || (col == 3) || (col == 5);
        c.push_bit(one);
    }
    c.finalize_block();

    EXPECT_EQ(c.lh_count(), 2U);

    EXPECT_EQ(c.lsm().value(0), 3); // row 0 has 3 ones
    EXPECT_EQ(c.lsm().value(1), 3); // row 1 has 3 ones
    EXPECT_EQ(c.lsm().value(2), 0);

    // LH byte vector should be 2 * 32 bytes after popping
    auto lh = c.pop_all_lh_bytes();
    EXPECT_EQ(lh.size(), 64U);

    // Popping again should yield 0
    auto lh2 = c.pop_all_lh_bytes();
    EXPECT_TRUE(lh2.empty());
}
