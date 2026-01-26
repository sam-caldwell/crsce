/*
 * @file bithashbuffer_seedhash_single_block_test.cpp
 * @brief Validate seedHash for single-block SHA-256 padding case.
 */
#include <cstdint>
/**
 * @file bithashbuffer_seedhash_single_block_test.cpp
 * @brief Seed hash is used correctly for the first row block.
 * © 2026 Sam Caldwell. See LICENSE.txt for details.
 */
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "../../../../include/common/BitHashBuffer/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

/**
 * @name BitHashBufferSeedHashSingleBlockTest.SingleBlockPaddingShortSeed
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(BitHashBufferSeedHashSingleBlockTest, SingleBlockPaddingShortSeed) {
    const std::string seed = "abc"; // <=55 triggers a single-block padding path
    const BitHashBuffer buf(seed);

    const std::vector<std::uint8_t> bytes(seed.begin(), seed.end());
    const auto expected = sha256_digest(bytes.data(), bytes.size());
    EXPECT_EQ(buf.seedHash(), expected);
}
