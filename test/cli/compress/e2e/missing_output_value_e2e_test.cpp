/**
 * @file missing_output_value_e2e_test.cpp
 * @brief E2E: compress CLI returns usage error on missing -out value.
 */
#include "compress/Cli/run.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name CompressCLI.MissingOutputValue
 * @brief Expect return 2 when -out has no value.
 */
TEST(CompressCLI, MissingOutputValue) {
  std::vector<std::string> av = {"compress", "-in", "some.bin", "-out"};
  std::vector<char*> argv; argv.reserve(av.size());
  for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::compress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 2);
}
