/**
 * @file input_missing_e2e_test.cpp
 * @brief E2E: decompress CLI returns 3 when input does not exist.
 */
#include "decompress/Cli/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.InputDoesNotExist
 * @brief Expect return 3 when an input file is missing.
 */
TEST(DecompressCLI, InputDoesNotExist) {
  const auto td = tmp_dir();
  const std::string out = td + "/decompress_cli_out.tmp";
  remove_file(out);
  std::vector<std::string> av = {"decompress", "-in", td+"/no_such_input.crsc", "-out", out};
  std::vector<char*> argv; argv.reserve(av.size());
  for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::decompress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 3);
  remove_file(out);
}
