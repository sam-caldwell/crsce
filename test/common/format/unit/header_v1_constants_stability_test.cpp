/**
 * @file header_v1_constants_stability_test.cpp
 * @brief Negative tests to enforce HeaderV1 constant stability unless explicitly allowed.
 */
#include "common/Format/HeaderV1.h"
#include <gtest/gtest.h>

using crsce::common::format::HeaderV1;

// These checks intentionally fail the build/tests if HeaderV1 constants
// change unexpectedly. To perform an intentional version/size change,
// define CRSCE_ALLOW_HEADER_V1_CHANGE in your build flags to bypass.
#ifndef CRSCE_ALLOW_HEADER_V1_CHANGE
static_assert(HeaderV1::kVersion == 1,
              "HeaderV1::kVersion changed unexpectedly. Define CRSCE_ALLOW_HEADER_V1_CHANGE to bypass during intended upgrade.");
static_assert(HeaderV1::kSize == 28,
              "HeaderV1::kSize changed unexpectedly. Define CRSCE_ALLOW_HEADER_V1_CHANGE to bypass during intended upgrade.");

TEST(HeaderV1ConstantsStability, VersionIs1) { // NOLINT(readability-identifier-naming)
    EXPECT_EQ(HeaderV1::kVersion, 1U);
}

TEST(HeaderV1ConstantsStability, SizeIs28) { // NOLINT(readability-identifier-naming)
    EXPECT_EQ(HeaderV1::kSize, 28U);
}
#else
TEST(HeaderV1ConstantsStability, Bypassed) { // NOLINT(readability-identifier-naming)
    GTEST_SKIP() << "CRSCE_ALLOW_HEADER_V1_CHANGE defined; stability checks bypassed.";
}
#endif
