/**
 * @file missing_output_value_e2e_test.cpp
 * @brief E2E: decompress CLI returns 2 on missing -out value.
 */
#include "decompress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.MissingOutputValue
 * @brief Expect return 2 for missing -out value.
 */
TEST(DecompressCLI, MissingOutputValue) {
    std::vector<std::string> av = {"decompress", "-in", "some.crsc", "-out"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::decompress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 2);
}
