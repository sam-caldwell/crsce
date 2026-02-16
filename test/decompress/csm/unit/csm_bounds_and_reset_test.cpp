/**
 * @file csm_bounds_and_reset_test.cpp
 */
#include <gtest/gtest.h>
#include <cstddef>
#include "decompress/Csm/Csm.h"
#include "common/exceptions/CsmIndexOutOfBounds.h"

using crsce::decompress::Csm;

/**
 * @name CsmBounds.MethodsThrowOnOutOfBounds
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CsmBounds, MethodsThrowOnOutOfBounds) { // NOLINT
    Csm cs;
    const std::size_t oob = Csm::kS; // 511 is out of bounds

    EXPECT_THROW({ auto v = cs.get(oob, 0); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW({ auto v = cs.get(0, oob); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.set(oob, 0), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.clear(0, oob), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.lock(oob, 1), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.lock(1, oob), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW({ auto v = cs.is_locked(oob, 2); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW({ auto v = cs.is_locked(2, oob); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.set_belief(oob, 3, 1.0), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW(cs.set_belief(3, oob, 1.0), crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW({ auto v = cs.get_data(oob, 4); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
    EXPECT_THROW({ auto v = cs.get_data(4, oob); (void)v; }, crsce::decompress::CsmIndexOutOfBounds);
}

/**
 * @name CsmReset.ClearsAllLayers
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CsmReset, ClearsAllLayers) { // NOLINT
    Csm cs;
    cs.set(0, 0);
    cs.lock(0, 0);
    cs.set_belief(0, 0, 42.0);
    ASSERT_TRUE(cs.get(0, 0));
    ASSERT_TRUE(cs.is_locked(0, 0));
    ASSERT_DOUBLE_EQ(cs.get_data(0, 0), 42.0);

    cs.reset();
    EXPECT_FALSE(cs.get(0, 0));
    EXPECT_FALSE(cs.is_locked(0, 0));
    EXPECT_DOUBLE_EQ(cs.get_data(0, 0), 0.0);
}
