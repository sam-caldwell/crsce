/**
 * @file compress_end_of_row_resets_next_row_test.cpp
 * @brief Verify end-of-row behavior resets column and advances to the next row.
 */
#include <gtest/gtest.h>

#include <cstddef>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

/**
 * @name CompressAdvanceCoords.EndOfRowDoesNotAdvanceColumnAndResetsNextRow
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressAdvanceCoords, EndOfRowDoesNotAdvanceColumnAndResetsNextRow) { // NOLINT
    Compress cx("in.bin", "out.crsc");
    // Fill row 0 with ones (511 data bits); end-of-row triggers pad and row advance
    for (std::size_t i = 0; i < Compress::kBitsPerRow; ++i) {
        cx.push_bit(true);
    }
    // Row sums and columns reflect a full row of ones
    EXPECT_EQ(cx.lsm().value(0), static_cast<crsce::common::CrossSum::ValueType>(Compress::kBitsPerRow));
    EXPECT_EQ(cx.vsm().value(0), 1);
    EXPECT_EQ(cx.vsm().value(Compress::kS - 1), 1);

    // Next bit should be at (row 1, col 0) — verify column 0 increments again
    cx.push_bit(true);
    EXPECT_EQ(cx.vsm().value(0), 2);
    // Column 1 should still be 1 after exactly one more bit in new row
    EXPECT_EQ(cx.vsm().value(1), 1);
}
