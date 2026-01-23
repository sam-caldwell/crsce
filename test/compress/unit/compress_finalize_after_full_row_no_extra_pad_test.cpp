/**
 * @file compress_finalize_after_full_row_no_extra_pad_test.cpp
 * @brief After exactly one full row (511 bits) pushed, finalize should not add another LH row.
 */
#include "compress/Compress/Compress.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>

using crsce::compress::Compress;

/**

 * @name CompressTest.FinalizeAfterFullRowNoExtraPad

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CompressTest, FinalizeAfterFullRowNoExtraPad) {
    Compress c{"in.bin", "out.bin"};
    // Push exactly one full row worth of bits
    for (std::size_t i = 0; i < Compress::kBitsPerRow; ++i) {
        c.push_bit((i % 7) == 0); // arbitrary pattern
    }
    // At this point, push_bit has already appended a pad bit and enqueued one LH digest
    ASSERT_EQ(c.lh_count(), 1U);

    // Now call finalize_block(); pad_and_finalize_row_if_needed() should early-return.
    c.finalize_block();
    EXPECT_EQ(c.lh_count(), 1U);

    // Pop LH and ensure exactly one digest is returned
    const auto lh = c.pop_all_lh_bytes();
    EXPECT_EQ(lh.size(), 32U);
    // No more LH bytes remain
    const auto lh2 = c.pop_all_lh_bytes();
    EXPECT_TRUE(lh2.empty());
}
