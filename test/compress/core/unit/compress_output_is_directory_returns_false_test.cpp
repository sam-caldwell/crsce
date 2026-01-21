/**
 * @file compress_output_is_directory_returns_false_test.cpp
 * @brief compress_file should return false if output path is a directory.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

TEST(CompressCore, OutputIsDirectoryReturnsFalse) { // NOLINT
  namespace fs = std::filesystem;
  const std::string in = std::string(TEST_BINARY_DIR) + "/c_outdir_in.bin";
  const std::string out = std::string(TEST_BINARY_DIR); // directory

  // Ensure input exists with non-zero length
  {
    std::ofstream f(in, std::ios::binary);
    ASSERT_TRUE(f.good());
    f.put(static_cast<char>(0xFF));
  }
  ASSERT_TRUE(fs::exists(in));
  ASSERT_GT(fs::file_size(in), 0U);

  Compress cx(in, out);
  EXPECT_FALSE(cx.compress_file());
}
