/**
 * @file invalid_header_fails_e2e_test.cpp
 * @brief E2E: decompress CLI returns 4 for invalid header input.
 */
#include "decompress/Cli/detail/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.InvalidHeaderFails
 * @brief Expect return 4 when input file exists but header invalid.
 */
TEST(DecompressCLI, InvalidHeaderFails) {
  const auto td = tmp_dir();
  const std::string in = td + "/decompress_cli_bad_header.crsc";
  const std::string out = td + "/decompress_cli_bad_header_out.bin";
  remove_file(in);
  remove_file(out);
  { const std::ofstream f(in, std::ios::binary); }

  std::vector<std::string> av = {"decompress", "-in", in, "-out", out};
  std::vector<char*> argv; argv.reserve(av.size()); for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::decompress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 4);

  remove_file(in);
  remove_file(out);
}
