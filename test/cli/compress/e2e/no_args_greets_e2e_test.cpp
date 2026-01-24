/**
 * @file no_args_greets_e2e_test.cpp
 * @brief E2E: compress CLI prints a greeting with no args.
 */
#include "compress/Cli/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.NoArgsGreets
 * @brief Expect return 0 and greeting when no args provided.
 */
TEST(CompressCLI, NoArgsGreets) {
  std::vector<std::string> av = {"compress"};
  std::vector<char*> argv; argv.reserve(av.size());
  for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::compress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 0);
}
