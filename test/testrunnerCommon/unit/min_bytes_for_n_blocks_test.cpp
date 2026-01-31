/**
 * @file min_bytes_for_n_blocks_test.cpp
 */
#include <gtest/gtest.h>

#include <cstdint>
#include "testrunner/detail/min_bytes_for_n_blocks.h"
#include "decompress/Csm/detail/Csm.h"

TEST(TestRunnerCommon, MinBytesForNBlocksBasicCases) {
    using crsce::testrunner::detail::min_bytes_for_n_blocks;
    constexpr std::uint64_t S = crsce::decompress::Csm::kS;
    constexpr std::uint64_t bits_per_block = S * S;

    EXPECT_EQ(min_bytes_for_n_blocks(0), 0ULL);
    // For 1 block, we need 1 bit -> 1 byte
    EXPECT_EQ(min_bytes_for_n_blocks(1), 1ULL);
    // For 2 blocks, need bits_per_block + 1 bits -> ceil((bits_per_block+1)/8)
    const std::uint64_t two = (bits_per_block + 1ULL + 7ULL) / 8ULL;
    EXPECT_EQ(min_bytes_for_n_blocks(2), two);
}

