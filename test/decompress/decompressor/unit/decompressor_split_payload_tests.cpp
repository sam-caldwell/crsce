/**
 * @file decompressor_split_payload_tests.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "decompress/Decompressor/Decompressor.h"

using crsce::decompress::Decompressor;

/**
 * @name Decompressor.SplitPayloadOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(Decompressor, SplitPayloadOk) { // NOLINT
    std::vector<std::uint8_t> block(Decompressor::kBlockBytes);
    for (std::size_t i = 0; i < block.size(); ++i) {
        block[i] = static_cast<std::uint8_t>(i & 0xFFU);
    }
    std::span<const std::uint8_t> lh;
    std::span<const std::uint8_t> sums;
    ASSERT_TRUE(Decompressor::split_payload(block, lh, sums));
    EXPECT_EQ(lh.size(), Decompressor::kLhBytes);
    EXPECT_EQ(sums.size(), Decompressor::kSumsBytes);
    EXPECT_EQ(lh.front(), static_cast<std::uint8_t>(0));
    EXPECT_EQ(sums.front(), static_cast<std::uint8_t>(Decompressor::kLhBytes & 0xFFU));
}

/**
 * @name Decompressor.SplitPayloadBadLengths
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(Decompressor, SplitPayloadBadLengths) { // NOLINT
    const std::vector<std::uint8_t> too_small(Decompressor::kBlockBytes - 1U);
    const std::vector<std::uint8_t> too_large(Decompressor::kBlockBytes + 1U);
    std::span<const std::uint8_t> lh;
    std::span<const std::uint8_t> sums;
    EXPECT_FALSE(Decompressor::split_payload(too_small, lh, sums));
    EXPECT_FALSE(Decompressor::split_payload(too_large, lh, sums));
}
