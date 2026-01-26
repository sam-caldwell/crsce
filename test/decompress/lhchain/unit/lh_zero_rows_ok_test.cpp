/**
 * @file lh_zero_rows_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/detail/Csm.h"

using crsce::decompress::LHChainVerifier;
using crsce::decompress::Csm;

/**

 * @name LHChainVerifier.ZeroRowsOk

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(LHChainVerifier, ZeroRowsOk) { // NOLINT
    const LHChainVerifier v{"CRSCE_v1_seed"};
    const Csm csm; // default zeros
    constexpr std::vector<std::uint8_t> empty{};
    EXPECT_TRUE(v.verify_rows(csm, empty, 0));
}
