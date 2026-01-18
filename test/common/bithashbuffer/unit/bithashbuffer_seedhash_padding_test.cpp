/*
 * @file bithashbuffer_seedhash_padding_test.cpp
 * @brief Validate seedHash for single- and double-block SHA-256 padding.
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <string>
#include <vector>

#include "common/BitHashBuffer.h"
#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::common::BitHashBuffer;
using crsce::common::detail::sha256::sha256_digest;

TEST(BitHashBufferSeedHashPaddingTest, TwoBlockPaddingLen56) {
    const std::string seed(56, 'A'); // 56 triggers two-block padding path
    const BitHashBuffer buf(seed);

    const std::vector<std::uint8_t> bytes(seed.begin(), seed.end());
    const auto expected = sha256_digest(bytes.data(), bytes.size());
    EXPECT_EQ(buf.seedHash(), expected);
}

// a single-block padding test moved to its own file to keep one TEST per file
