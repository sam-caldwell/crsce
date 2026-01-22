/**
 * @file lhchain_bithashbuffer_integration_test.cpp
 * @brief End-to-end: BitHashBuffer produces LH chain for 511 rows; LHChainVerifier validates against CSM.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "common/BitHashBuffer/BitHashBuffer.h"
#include "decompress/Csm/Csm.h"
#include "decompress/LHChainVerifier/LHChainVerifier.h"

using crsce::common::BitHashBuffer;
using crsce::decompress::Csm;
using crsce::decompress::LHChainVerifier;

/**

 * @name LHChainIntegration.BitHashBufferEndToEndChainVerification

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(LHChainIntegration, BitHashBufferEndToEndChainVerification) { // NOLINT
    constexpr std::string seed = "CRSCE_v1_seed";
    BitHashBuffer hasher(seed);
    const LHChainVerifier verifier(seed);
    Csm csm;

    // Deterministic pseudo-random generator
    std::mt19937 rng(0xC0FFEEU);
    std::bernoulli_distribution bit_dist(0.5);

    // Generate 511 rows of 511 data bits each; push a trailing 0 pad bit per row
    for (std::size_t r = 0; r < Csm::kS; ++r) {
        for (std::size_t c = 0; c < Csm::kS; ++c) {
            const bool bit = bit_dist(rng);
            csm.put(r, c, bit);
            hasher.pushBit(bit);
        }
        // Pad bit (0) to complete 64 bytes for the row
        hasher.pushBit(false);
    }

    // Collect all 511 chained digests into a contiguous byte vector
    ASSERT_EQ(hasher.count(), Csm::kS);
    std::vector<std::uint8_t> lh_bytes;
    lh_bytes.reserve(Csm::kS * BitHashBuffer::kHashSize);
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        const auto h = hasher.popHash();
        ASSERT_TRUE(h.has_value());
        lh_bytes.insert(lh_bytes.end(), h->begin(), h->end());
    }
    ASSERT_EQ(lh_bytes.size(), Csm::kS * BitHashBuffer::kHashSize);

    // Verify the entire chain against the CSM rows
    EXPECT_TRUE(verifier.verify_all(csm, lh_bytes));
}
