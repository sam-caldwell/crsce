/**
 * @file compress_zero_length_header_only_test.cpp
 * @brief Zero-length input should write only a v1 header (28 bytes) and succeed.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <cstdio>
#include <string>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

TEST(CompressIntegration, ZeroLengthWritesHeaderOnly) { // NOLINT
  namespace fs = std::filesystem;
  const std::string in = std::string(TEST_BINARY_DIR) + "/c_zero_len_in.bin";
  const std::string out = std::string(TEST_BINARY_DIR) + "/c_zero_len_out.crsc";

  // Ensure clean slate
  std::remove(in.c_str());
  std::remove(out.c_str());

  // Create empty input file
  {
    const std::ofstream f(in, std::ios::binary);
    ASSERT_TRUE(f.good());
  }
  ASSERT_TRUE(fs::exists(in));
  ASSERT_EQ(fs::file_size(in), 0U);

  Compress cx(in, out);
  ASSERT_TRUE(cx.compress_file());

  ASSERT_TRUE(fs::exists(out));
  // v1 header is fixed 28 bytes; no payload expected
  EXPECT_EQ(fs::file_size(out), 28U);
}
