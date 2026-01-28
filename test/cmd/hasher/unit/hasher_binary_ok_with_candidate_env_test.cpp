/**
 * @file hasher_binary_ok_with_candidate_env_test.cpp
 * @brief Unit: run hasher binary with CRSCE_HASHER_CANDIDATE pointing to ok helper; expect exit 0.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

/**
 * @name HasherBinary.OkWithCandidateEnv
 * @brief Expect hasher returns 0 when candidate override produces correct digest.
 */
TEST(HasherBinary, OkWithCandidateEnv) { // NOLINT(readability-identifier-naming)
    const std::string helper = std::string(TEST_BINARY_DIR) + "/sha256_ok_helper";
    (void) ::unsetenv("CRSCE_HASHER_CMD"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    ASSERT_EQ(::setenv("CRSCE_HASHER_CANDIDATE", helper.c_str(), 1), 0); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)

    const std::string exe = std::string(TEST_BINARY_DIR) + "/hasher";
    const int rc = std::system(exe.c_str()); // NOLINT(concurrency-mt-unsafe)
    EXPECT_EQ(rc, 0) << "hasher should exit 0 using candidate override";

    (void) ::unsetenv("CRSCE_HASHER_CANDIDATE"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
}
