/*
 * @file bithashbuffer_seedhash_single_block_test.cpp
 * @brief Validate seedHash for single-block SHA-256 padding case.
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "common/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

TEST(BitHashBufferSeedHashSingleBlockTest, SingleBlockPaddingShortSeed) {
    const std::string seed = "abc"; // <=55 triggers a single-block padding path
    const BitHashBuffer buf(seed);

    const std::vector<std::uint8_t> bytes(seed.begin(), seed.end());
    const auto expected = sha256_digest(bytes.data(), bytes.size());
    EXPECT_EQ(buf.seedHash(), expected);
}
