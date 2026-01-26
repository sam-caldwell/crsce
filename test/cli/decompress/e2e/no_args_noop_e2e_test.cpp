/**
 * @file no_args_noop_e2e_test.cpp
 * @brief E2E: decompress CLI returns 0 when no args provided.
 */
#include "decompress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.NoArgsNoOp
 * @brief Expect return 0 when no args provided.
 */
TEST(DecompressCLI, NoArgsNoOp) {
    std::vector<std::string> av = {"decompress"};
    std::vector<char *> argv;
    argv.reserve(av.size());
    for (auto &s: av) { argv.push_back(s.data()); }
    const int rc = crsce::decompress::cli::run(std::span<char *>{argv.data(), argv.size()});
    EXPECT_EQ(rc, 0);
}
