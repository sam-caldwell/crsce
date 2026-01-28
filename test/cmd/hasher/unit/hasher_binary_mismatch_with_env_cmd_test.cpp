/**
 * @file hasher_binary_mismatch_with_env_cmd_test.cpp
 * @brief Unit: run hasher binary with CRSCE_HASHER_CMD pointing to bad helper; expect non-zero.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

/**
 * @name HasherBinary.MismatchWithEnvCmd
 * @brief Expect hasher returns 1 when control tool produces mismatched digest.
 */
TEST(HasherBinary, MismatchWithEnvCmd) { // NOLINT(readability-identifier-naming)
    const std::string helper = std::string(TEST_BINARY_DIR) + "/sha256_bad_helper";
    ASSERT_EQ(::setenv("CRSCE_HASHER_CMD", helper.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

    const std::string exe = std::string(TEST_BINARY_DIR) + "/hasher";
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    EXPECT_NE(rc, 0) << "hasher should exit non-zero when control mismatches candidate";

    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe)
}
