/**
 * @file lh_zero_rows_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/Csm.h"

using crsce::decompress::RowHashVerifier;
using crsce::decompress::Csm;

/**
 * @name RowHashVerifier.ZeroRowsOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(RowHashVerifier, ZeroRowsOk) { // NOLINT
    const RowHashVerifier v{};
    const Csm csm; // default zeros
    constexpr std::vector<std::uint8_t> empty{};
    EXPECT_TRUE(v.verify_rows(csm, empty, 0));
}
