/**
 * @file bits_full_coverage_test.cpp
 */
#include <gtest/gtest.h>
#include <cstdint>
#include "decompress/Csm/detail/Bits.h"

using crsce::decompress::Bits;

TEST(BitsFull, ConstructorsAndAccessors) {
    const Bits b0; // default
    EXPECT_FALSE(b0.data());
    EXPECT_FALSE(b0.resolved());
    EXPECT_FALSE(b0.mu_locked());
    EXPECT_EQ(b0.raw(), 0U);

    const Bits b1(true, true, true);
    EXPECT_TRUE(b1.data());
    EXPECT_TRUE(b1.resolved());
    EXPECT_TRUE(b1.mu_locked());
    EXPECT_NE(b1.raw(), 0U);

    const Bits b2(static_cast<std::uint8_t>(0xFF)); // masked in ctor
    EXPECT_TRUE(b2.data());
    EXPECT_TRUE(b2.resolved());
    EXPECT_TRUE(b2.mu_locked());
}

TEST(BitsFull, MutatorsAndRawOps) {
    Bits b;
    b.set_data(true); EXPECT_TRUE(b.data());
    b.set_resolved(true); EXPECT_TRUE(b.resolved());
    b.set_mu_locked(true); EXPECT_TRUE(b.mu_locked());

    b.flip_data(); EXPECT_FALSE(b.data());
    b.set_raw(0U); EXPECT_EQ(b.raw(), 0U);

    b.assign(true, false, true);
    EXPECT_TRUE(b.data()); EXPECT_FALSE(b.resolved()); EXPECT_TRUE(b.mu_locked());

    b.clear(); EXPECT_EQ(b.raw(), 0U);
}

TEST(BitsFull, MuLockTryAndUnlock) {
    Bits b;
    EXPECT_FALSE(b.mu_locked());
    const bool got1 = b.try_lock_mu();
    EXPECT_TRUE(got1);
    EXPECT_TRUE(b.mu_locked());
    // Second acquisition should fail while held
    EXPECT_FALSE(b.try_lock_mu());
    b.unlock_mu();
    EXPECT_FALSE(b.mu_locked());
}

TEST(BitsFull, Operators) {
    Bits b;
    EXPECT_FALSE(static_cast<bool>(b));
    b = true; // operator=(bool)
    EXPECT_TRUE(static_cast<bool>(b));

    Bits c(true, false, false);
    EXPECT_TRUE(b == c);
    c.set_resolved(true);
    EXPECT_FALSE(b == c);
}
