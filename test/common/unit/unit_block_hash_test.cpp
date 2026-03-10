/**
 * @file unit_block_hash_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the BlockHash class.
 */
#include <gtest/gtest.h>

#include <array>

#include "common/BlockHash/BlockHash.h"
#include "common/Csm/Csm.h"

namespace {

    using crsce::common::BlockHash;
    using crsce::common::Csm;

    /**
     * @brief Verify that compute() returns the same hash for identical CSM instances.
     *
     * Computing the hash of an all-zeros CSM twice should produce the same 32-byte
     * SHA-256 digest, demonstrating determinism.
     */
    TEST(BlockHashTest, ComputeIsDeterministic) {
        const Csm csm1;
        const Csm csm2;

        const auto hash1 = BlockHash::compute(csm1);
        const auto hash2 = BlockHash::compute(csm2);

        EXPECT_EQ(hash1, hash2);
    }

    /**
     * @brief Verify that verify() returns true when the CSM matches the expected hash.
     *
     * Compute a hash from a CSM, then verify that the same CSM validates against
     * that hash.
     */
    TEST(BlockHashTest, VerifyReturnsTrueForCorrectHash) {
        const Csm csm;
        const auto expected_hash = BlockHash::compute(csm);

        const bool is_valid = BlockHash::verify(csm, expected_hash);
        EXPECT_TRUE(is_valid);
    }

    /**
     * @brief Verify that verify() returns false when the hash is tampered.
     *
     * Compute a hash from a CSM, tamper with one byte of the hash, then verify
     * that the CSM no longer validates against the corrupted hash.
     */
    TEST(BlockHashTest, VerifyReturnsFalseForWrongHash) {
        const Csm csm;
        auto wrong_hash = BlockHash::compute(csm);

        // Tamper with the first byte of the hash
        wrong_hash[0] ^= 0xFF;

        const bool is_valid = BlockHash::verify(csm, wrong_hash);
        EXPECT_FALSE(is_valid);
    }

    /**
     * @brief Verify that different CSM instances produce different hashes.
     *
     * An all-zeros CSM and a CSM with one cell set to 1 should have different
     * SHA-256 hashes.
     */
    TEST(BlockHashTest, DifferentCsmsDifferentHashes) {
        const Csm csm_zeros;
        Csm csm_one_bit_set;
        csm_one_bit_set.set(100, 200, 1);

        const auto hash_zeros = BlockHash::compute(csm_zeros);
        const auto hash_one_bit = BlockHash::compute(csm_one_bit_set);

        EXPECT_NE(hash_zeros, hash_one_bit);
    }

} // namespace
