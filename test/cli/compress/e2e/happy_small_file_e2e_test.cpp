/**
 * @file happy_small_file_e2e_test.cpp
 * @brief E2E: compress CLI compresses a small file successfully.
 */
#include "compress/Cli/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/**
 * @name CompressCLI.HappySmallFile
 * @brief Expect return 0 and output a file created.
 */
TEST(CompressCLI, HappySmallFile) {
  const auto td = tmp_dir();
  const std::string in = td + "/compress_cli_in_ok.bin";
  const std::string out = td + "/compress_cli_out_ok.crsc";
  remove_file(in);
  remove_file(out);

  {
    std::ofstream f(in, std::ios::binary);
    std::string payload = "abcdefg";
    f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  }

  std::vector<std::string> av = {"compress", "-in", in, "-out", out};
  std::vector<char*> argv; argv.reserve(av.size());
  for (auto &s : av) { argv.push_back(s.data()); }
  const int rc = crsce::compress::cli::run(std::span<char*>{argv.data(), argv.size()});
  EXPECT_EQ(rc, 0);
  EXPECT_TRUE(fs::exists(out));

  remove_file(in);
  remove_file(out);
}
