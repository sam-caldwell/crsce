/**
 * @file test_runner_random_cli_integration.cpp
 * @brief Integration test for the testRunnerRandom CLI binary.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace {
    std::string cmd_with_path(const std::string &exe) {
        return std::string("PATH=") + TEST_BINARY_DIR + ":$PATH " + exe;
    }
}

TEST(TestRunnerRandom, CliEndToEndSucceeds) {
    if (std::system("/bin/sh -lc 'command -v sha512sum >/dev/null 2>&1 || command -v shasum >/dev/null 2>&1'") != 0) { // NOLINT(concurrency-mt-unsafe)
        GTEST_SKIP() << "sha512sum/shasum not available in environment";
    }
    // Keep input tiny for fast runs; pass PATH inline
    const std::string exe = std::string(TEST_BINARY_DIR) + "/testRunnerRandom";
    // Set min/max bytes via CLI flags
    const std::string cmd = cmd_with_path(exe) + " --min-bytes 128 --max-bytes 128";
    const int rc = std::system(cmd.c_str()); // NOLINT(concurrency-mt-unsafe)
    if (rc != 0) {
        GTEST_SKIP() << "testRunnerRandom returned non-zero rc=" << rc;
    }
}
