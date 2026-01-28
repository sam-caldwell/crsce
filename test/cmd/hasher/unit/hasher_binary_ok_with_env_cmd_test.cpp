/**
 * @file hasher_binary_ok_with_env_cmd_test.cpp
 * @brief Unit: run hasher binary with CRSCE_HASHER_CMD pointing to ok helper; expect exit 0.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

/**
 * @name HasherBinary.OkWithEnvCmd
 * @brief Expect hasher returns 0 when control tool produces correct digest.
 */
TEST(HasherBinary, OkWithEnvCmd) { // NOLINT(readability-identifier-naming)
    const std::string helper = std::string(TEST_BINARY_DIR) + "/sha256_ok_helper";
    ASSERT_EQ(::setenv("CRSCE_HASHER_CMD", helper.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

    const std::string exe = std::string(TEST_BINARY_DIR) + "/hasher";
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    EXPECT_EQ(rc, 0) << "hasher should exit 0 when control matches candidate";

    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
