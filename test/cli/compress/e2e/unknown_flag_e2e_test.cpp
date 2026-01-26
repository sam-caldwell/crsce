/**
 * @file unknown_flag_e2e_test.cpp
 * @brief E2E: compress CLI returns usage error on unknown flag.
 */
#include "compress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.UnknownFlag
 * @brief Expect return 2 for unknown flag.
 */
TEST(CompressCLI, UnknownFlag) {
    std::vector<std::string> av = {"compress", "--bogus"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::compress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 2);
}
