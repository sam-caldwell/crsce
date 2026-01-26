/**
 * @file roundtrip_small_file_e2e_test.cpp
 * @brief E2E: compress then decompress; bytes match exactly.
 */
#include "decompress/Cli/detail/run.h"
#include "compress/Cli/detail/run.h"
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <vector>

/**
 * @name DecompressCLI.RoundtripSmallFile
 * @brief Expect return 0 and identical bytes after roundtrip.
 */
TEST(DecompressCLI, RoundtripSmallFile) {
  const auto td = tmp_dir();
  const std::string raw = td + "/decompress_cli_raw_ok.bin";
  const std::string cont = td + "/decompress_cli_cont_ok.crsc";
  const std::string out = td + "/decompress_cli_out_ok.bin";
  remove_file(raw);
  remove_file(cont);
  remove_file(out);

  {
    std::ofstream f(raw, std::ios::binary);
    std::string payload = "Hello, CRSCE!";
    f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  }

  {
    std::vector<std::string> cav = {"compress", "-in", raw, "-out", cont};
    std::vector<char*> cargv; cargv.reserve(cav.size()); for (auto &s : cav) { cargv.push_back(s.data()); }
    ASSERT_EQ(crsce::compress::cli::run(std::span<char*>{cargv.data(), cargv.size()}), 0);
  }

  {
    std::vector<std::string> dav = {"decompress", "-in", cont, "-out", out};
    std::vector<char*> dargv; dargv.reserve(dav.size()); for (auto &s : dav) { dargv.push_back(s.data()); }
    ASSERT_EQ(crsce::decompress::cli::run(std::span<char*>{dargv.data(), dargv.size()}), 0);
  }

  std::ifstream a(raw, std::ios::binary); std::ifstream b(out, std::ios::binary);
  std::vector<char> abuf((std::istreambuf_iterator<char>(a)), {});
  std::vector<char> bbuf((std::istreambuf_iterator<char>(b)), {});
  ASSERT_EQ(abuf.size(), bbuf.size());
  EXPECT_TRUE(std::equal(abuf.begin(), abuf.end(), bbuf.begin()));

  remove_file(raw);
  remove_file(cont);
  remove_file(out);
}
