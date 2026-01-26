/**
 * @file csm_data_layer_test.cpp
 */
#include <gtest/gtest.h>
#include <numbers>
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::Csm;

/**

 * @name CsmData.DefaultZeroAndSetGet

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(CsmData, DefaultZeroAndSetGet) { // NOLINT
    Csm cs;
    EXPECT_DOUBLE_EQ(cs.get_data(0, 0), 0.0);
    EXPECT_DOUBLE_EQ(cs.get_data(510, 510), 0.0);

    cs.set_data(0, 0, std::numbers::pi);
    cs.set_data(100, 200, -2.5);
    cs.set_data(510, 510, 1.0e-6);

    EXPECT_DOUBLE_EQ(cs.get_data(0, 0), std::numbers::pi);
    EXPECT_DOUBLE_EQ(cs.get_data(100, 200), -2.5);
    EXPECT_DOUBLE_EQ(cs.get_data(510, 510), 1.0e-6);
}
