/**
 * @file usage_help_e2e_test.cpp
 * @brief E2E: compress CLI shows usage on -h.
 */
#include "compress/Cli/detail/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.UsageHelp
 * @brief Expect return 0 when -h is provided.
 */
TEST(CompressCLI, UsageHelp) {
  std::vector<std::string> av = {"compress", "-h"};
  std::vector<char*> argv; argv.reserve(av.size());
  for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::compress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 0);
}
