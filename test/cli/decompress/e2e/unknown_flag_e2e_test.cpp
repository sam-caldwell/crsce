/**
 * @file unknown_flag_e2e_test.cpp
 * @brief E2E: decompress CLI returns 2 for unknown flag.
 */
#include "decompress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.UnknownFlag
 * @brief Expect return 2 for unknown flag.
 */
TEST(DecompressCLI, UnknownFlag) {
    std::vector<std::string> av = {"decompress", "--bogus"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::decompress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 2);
}
