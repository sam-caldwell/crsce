/**
 * @file unit_sha1_hash_verifier_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the Sha1HashVerifier class.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "decompress/Solvers/Sha1HashVerifier.h"

using crsce::decompress::solvers::Sha1HashVerifier;

namespace {
    constexpr std::uint16_t kS = 127;
} // namespace

/**
 * @brief Compute hash twice on all-zeros row and verify deterministic result.
 */
TEST(Sha1HashVerifierTest, ComputeHashReturnsDeterministicResult) {
    const Sha1HashVerifier verifier(kS);
    const std::array<std::uint64_t, 2> row{};

    const auto hash1 = verifier.computeHash(row);
    const auto hash2 = verifier.computeHash(row);

    EXPECT_EQ(hash1, hash2);
}

/**
 * @brief Verify that bytes [20..31] are zero-padded in the returned hash.
 */
TEST(Sha1HashVerifierTest, ComputeHashZeroPadsTo32Bytes) {
    const Sha1HashVerifier verifier(kS);
    const std::array<std::uint64_t, 2> row{};

    const auto hash = verifier.computeHash(row);

    // SHA-1 digest is 20 bytes, padded to 32 bytes with zeros
    for (std::size_t i = Sha1HashVerifier::kSha1DigestBytes; i < hash.size(); ++i) {
        EXPECT_EQ(hash.at(i), 0U); // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
}

/**
 * @brief Set expected hash from computeHash and verify returns true.
 */
TEST(Sha1HashVerifierTest, VerifyRowReturnsTrueForCorrectHash) {
    Sha1HashVerifier verifier(kS);
    const std::array<std::uint64_t, 2> row{};
    constexpr std::uint16_t rowIndex = 0;

    const auto expectedHash = verifier.computeHash(row);
    verifier.setExpected(rowIndex, expectedHash);

    EXPECT_TRUE(verifier.verifyRow(rowIndex, row));
}

/**
 * @brief Set expected hash, tamper one byte, and verify returns false.
 */
TEST(Sha1HashVerifierTest, VerifyRowReturnsFalseForWrongHash) {
    Sha1HashVerifier verifier(kS);
    const std::array<std::uint64_t, 2> row{};
    constexpr std::uint16_t rowIndex = 0;

    auto tamperedHash = verifier.computeHash(row);
    tamperedHash.at(0) ^= 0x01;
    verifier.setExpected(rowIndex, tamperedHash);

    EXPECT_FALSE(verifier.verifyRow(rowIndex, row));
}

/**
 * @brief Two different row inputs produce different hashes.
 */
TEST(Sha1HashVerifierTest, DifferentRowsDifferentHashes) {
    const Sha1HashVerifier verifier(kS);

    const std::array<std::uint64_t, 2> row1{};
    std::array<std::uint64_t, 2> row2{};
    row2.at(0) = 0x8000000000000000ULL;

    const auto hash1 = verifier.computeHash(row1);
    const auto hash2 = verifier.computeHash(row2);

    EXPECT_NE(hash1, hash2);
}
