/**
 * @file missing_input_value_e2e_test.cpp
 * @brief E2E: decompress CLI returns insufficient-args (4) on missing -in value.
 */
#include "decompress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.MissingInputValue
 * @brief Expect return 4 for missing -in value.
 */
TEST(DecompressCLI, MissingInputValue) {
    std::vector<std::string> av = {"decompress", "-in"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::decompress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 4);
}
