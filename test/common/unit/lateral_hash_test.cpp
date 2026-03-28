/**
 * @file lateral_hash_test.cpp
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief Unit tests for the LateralHash class.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>

#include "common/LateralHash/LateralHash.h"

using crsce::common::LateralHash;

/**
 * @brief Verify that compute() on a known all-zero row produces a deterministic digest.
 *
 * An all-zero row is 64 zero bytes.  The SHA-1 of 64 zero bytes is a fixed, well-known
 * value.  We verify that compute() returns the correct digest.
 */
TEST(LateralHashTest, ComputeAllZeroRowIsDeterministic) {
    const std::array<std::uint64_t, 2> zeroRow{};
    const auto digest = LateralHash::compute(zeroRow);

    // CRC-32 of 16 zero bytes — must be deterministic and non-zero.
    EXPECT_EQ(digest.size(), LateralHash::kDigestBytes);
    const auto d2 = LateralHash::compute(zeroRow);
    EXPECT_EQ(digest, d2);
}

/**
 * @brief Verify that compute() on the same input always returns the same output.
 */
TEST(LateralHashTest, ComputeIsDeterministic) {
    std::array<std::uint64_t, 2> row{};
    row[0] = 0xDEADBEEFCAFEBABEULL;
    row[1] = 0x0123456789ABCDEFULL;

    const auto d1 = LateralHash::compute(row);
    const auto d2 = LateralHash::compute(row);
    EXPECT_EQ(d1, d2);
}

/**
 * @brief Verify that compute() on different inputs produces different digests.
 */
TEST(LateralHashTest, ComputeDifferentInputsDifferentDigests) {
    const std::array<std::uint64_t, 2> zeroRow{};

    std::array<std::uint64_t, 2> oneRow{};
    oneRow[0] = 1;

    const auto d0 = LateralHash::compute(zeroRow);
    const auto d1 = LateralHash::compute(oneRow);
    EXPECT_NE(d0, d1);
}

/**
 * @brief Verify that verify() returns true when digest matches the stored value.
 */
TEST(LateralHashTest, VerifyReturnsTrueForCorrectHash) {
    LateralHash lh(4);
    const std::array<std::uint64_t, 2> row{};
    const auto digest = LateralHash::compute(row);

    lh.store(0, digest);
    EXPECT_TRUE(lh.verify(0, digest));
}

/**
 * @brief Verify that verify() returns false when digest does not match the stored value.
 */
TEST(LateralHashTest, VerifyReturnsFalseForIncorrectHash) {
    LateralHash lh(4);
    const std::array<std::uint64_t, 2> row{};
    const auto digest = LateralHash::compute(row);

    lh.store(0, digest);

    // Tamper with one byte
    auto bad = digest;
    bad[0] ^= 0xFF;
    EXPECT_FALSE(lh.verify(0, bad));
}

/**
 * @brief Verify that verify() returns false for an unstored (zero-initialized) slot
 *        when compared against a non-zero digest.
 */
TEST(LateralHashTest, VerifyReturnsFalseForUnstoredSlot) {
    const LateralHash lh(4);
    const std::array<std::uint64_t, 2> row{};
    const auto digest = LateralHash::compute(row);

    // Row 1 was never stored (defaults to all zeros), so it should not match a real digest.
    EXPECT_FALSE(lh.verify(1, digest));
}

/**
 * @brief Verify that store()/getDigest() round-trip preserves the digest.
 */
TEST(LateralHashTest, StoreGetDigestRoundTrip) {
    LateralHash lh(8);
    const std::array<std::uint64_t, 2> row{};
    const auto digest = LateralHash::compute(row);

    lh.store(3, digest);
    const auto retrieved = lh.getDigest(3);
    EXPECT_EQ(digest, retrieved);
}

/**
 * @brief Verify that getDigest() returns a zero digest for a slot that was never stored.
 */
TEST(LateralHashTest, GetDigestReturnsZeroForUnstoredSlot) {
    const LateralHash lh(4);
    const auto retrieved = lh.getDigest(2);
    const std::array<std::uint8_t, LateralHash::kDigestBytes> zero{};
    EXPECT_EQ(retrieved, zero);
}

/**
 * @brief Verify that store() throws std::out_of_range for an out-of-bounds row index.
 */
TEST(LateralHashTest, StoreThrowsOutOfRange) {
    LateralHash lh(4);
    const std::array<std::uint8_t, LateralHash::kDigestBytes> digest{};
    EXPECT_THROW(lh.store(4, digest), std::out_of_range);
    EXPECT_THROW(lh.store(100, digest), std::out_of_range);
}

/**
 * @brief Verify that verify() throws std::out_of_range for an out-of-bounds row index.
 */
TEST(LateralHashTest, VerifyThrowsOutOfRange) {
    const LateralHash lh(4);
    const std::array<std::uint8_t, LateralHash::kDigestBytes> digest{};
    EXPECT_THROW((void)lh.verify(4, digest), std::out_of_range);
}

/**
 * @brief Verify that getDigest() throws std::out_of_range for an out-of-bounds row index.
 */
TEST(LateralHashTest, GetDigestThrowsOutOfRange) {
    const LateralHash lh(4);
    EXPECT_THROW((void)lh.getDigest(4), std::out_of_range);
}

/**
 * @brief Verify that the constructor throws std::invalid_argument when s is zero.
 */
TEST(LateralHashTest, ConstructorThrowsForZeroS) {
    EXPECT_THROW(const LateralHash lh(0), std::invalid_argument);
}
