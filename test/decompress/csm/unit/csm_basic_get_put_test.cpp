/**
 * @file csm_basic_get_put_test.cpp
 */
#include <gtest/gtest.h>
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::Csm;

/**

 * @name CsmBasic.DefaultsAreZero

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CsmBasic, DefaultsAreZero) { // NOLINT
    const Csm cs;
    // Sample a few coordinates including corners and center
    EXPECT_FALSE(cs.get(0, 0));
    EXPECT_FALSE(cs.get(Csm::kS - 1, Csm::kS - 1));
    EXPECT_FALSE(cs.get(128, 256));
}

/**

 * @name CsmBasic.PutAndGetRoundtrip

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CsmBasic, PutAndGetRoundtrip) { // NOLINT
    Csm cs;
    // Set bits at various positions and verify
    cs.put(0, 0, true);
    cs.put(0, 1, true);
    cs.put(0, 2, false); // explicit false should keep zero
    cs.put(510, 510, true);
    cs.put(255, 255, true);

    EXPECT_TRUE(cs.get(0, 0));
    EXPECT_TRUE(cs.get(0, 1));
    EXPECT_FALSE(cs.get(0, 2));
    EXPECT_TRUE(cs.get(510, 510));
    EXPECT_TRUE(cs.get(255, 255));

    // Toggle back to zero
    cs.put(255, 255, false);
    EXPECT_FALSE(cs.get(255, 255));
}
