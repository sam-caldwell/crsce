/**
 * @file fbits_empty_file_test.cpp
 * @brief One-test file: Empty file -> no bits.
 */
#include "common/FileBitSerializer.h"
#include "helpers/remove_file.h"
#include "helpers/tmp_dir.h"
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <string>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, EmptyFileHasNoBits) {
  const std::string empty_path = tmp_dir() + "/empty.tmp";
  {
    std::ofstream(empty_path, std::ios::binary);
  }
  FileBitSerializer serializer(empty_path); // NOLINT(misc-const-correctness)
  EXPECT_TRUE(serializer.good());
  EXPECT_FALSE(serializer.has_next());
  auto b = serializer.pop();
  EXPECT_FALSE(b.has_value());
  EXPECT_FALSE(serializer.has_next());
  EXPECT_FALSE(serializer.pop().has_value());
  remove_file(empty_path);
}
