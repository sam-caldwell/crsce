/**
 * @file missing_input_value_e2e_test.cpp
 * @brief E2E: compress CLI returns insufficient-args (4) on missing -in value.
 */
#include "compress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.MissingInputValue
 * @brief Expect return 4 when -in has no value.
 */
TEST(CompressCLI, MissingInputValue) {
    std::vector<std::string> av = {"compress", "-in"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::compress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 4);
}
