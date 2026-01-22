/**
 * @file lh_zero_rows_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/Csm.h"

using crsce::decompress::LHChainVerifier;
using crsce::decompress::Csm;

TEST(LHChainVerifier, ZeroRowsOk) { // NOLINT
    const LHChainVerifier v{"CRSCE_v1_seed"};
    const Csm csm; // default zeros
    constexpr std::vector<std::uint8_t> empty{};
    EXPECT_TRUE(v.verify_rows(csm, empty, 0));
}
