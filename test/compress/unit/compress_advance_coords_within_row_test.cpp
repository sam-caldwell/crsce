/**
 * @file compress_advance_coords_within_row_test.cpp
 * @brief Verify advancing column within a row updates cross-sums correctly.
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

/**
 * @name CompressAdvanceCoords.AdvancesColumnWithinRow
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressAdvanceCoords, AdvancesColumnWithinRow) { // NOLINT
    Compress cx("in.bin", "out.crsc");
    // Push two ones; expect column cross-sums at c=0 and c=1 incremented
    cx.push_bit(true);
    cx.push_bit(true);

    EXPECT_EQ(cx.lsm().value(0), 2); // two ones in row 0
    EXPECT_EQ(cx.vsm().value(0), 1); // col 0 incremented once
    EXPECT_EQ(cx.vsm().value(1), 1); // col 1 incremented once (advanced)
    EXPECT_EQ(cx.dsm().value(0), 1); // (r+c) mod 511 = 0 for (0,0)
    EXPECT_EQ(cx.dsm().value(1), 1); // (r+c) mod 511 = 1 for (0,1)
    EXPECT_EQ(cx.xsm().value(0), 1); // x = r-c = 0 for (0,0)
    EXPECT_EQ(cx.xsm().value(Compress::kS - 1), 1); // x = 510 for (0,1)
}
