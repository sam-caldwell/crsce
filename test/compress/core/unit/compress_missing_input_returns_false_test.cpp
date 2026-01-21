/**
 * @file compress_missing_input_returns_false_test.cpp
 * @brief compress_file should return false if input does not exist.
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include <string>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

TEST(CompressCore, MissingInputReturnsFalse) { // NOLINT
  namespace fs = std::filesystem;
  const std::string in = std::string(TEST_BINARY_DIR) + "/c_missing_in.bin";
  const std::string out = std::string(TEST_BINARY_DIR) + "/c_missing_out.crsc";

  std::remove(in.c_str());
  std::remove(out.c_str());
  ASSERT_FALSE(fs::exists(in));

  Compress cx(in, out);
  EXPECT_FALSE(cx.compress_file());
  EXPECT_FALSE(fs::exists(out));
}
