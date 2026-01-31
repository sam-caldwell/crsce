/**
 * @file alternating01_cross_sums_patterns_unit.cpp
 * @brief Verify cross-sum patterns for a full Alternating01 block using the compressor accumulator.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include "common/CrossSum/CrossSum.h"

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

/**
 * @name Alternating01.CrossSumsPatternsFullBlockUnit
 * @brief Intent: exercise expected cross-sum patterns for Alternating01 across a full 511x511 block.
 */
TEST(Alternating01, CrossSumsPatternsFullBlockUnit) { // NOLINT
    Compress cx{"in.bin", "out.bin"};
    constexpr std::size_t S = Compress::kS;

    // Stream a full block of Alternating01 bits row-major: bit(r,c) = (r + c) % 2
    std::size_t r = 0;
    std::size_t c = 0;
    for (std::size_t i = 0; i < S * S; ++i) {
        const bool bit = ((r + c) & 1U) != 0U;
        cx.push_bit(bit);
        // advance our coordinates like the compressor (column advances within row)
        ++c; if (c >= S) { c = 0; ++r; }
    }
    cx.finalize_block();

    // LSM/VSM alternate 255/256 with index parity
    for (std::size_t i = 0; i < S; ++i) {
        const auto expect = static_cast<crsce::common::CrossSum::ValueType>(255U + (i & 1U));
        EXPECT_EQ(cx.lsm().value(i), expect) << "LSM row " << i;
        EXPECT_EQ(cx.vsm().value(i), expect) << "VSM col " << i;
    }

    // DSM/XSM patterns are covered elsewhere; focus here on LSM/VSM corrections.
}
