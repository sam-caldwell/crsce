/**
 * @file fbits_missing_file_test.cpp
 * @brief One-test file: Non-existent file -> not good, no bits.
 */
#include "common/FileBitSerializer.h"
#include "helpers/tmp_dir.h"
#include <gtest/gtest.h>
#include <string>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty) {
  const std::string missing_path = tmp_dir() + "/definitely_not_here_12345.bin";
  FileBitSerializer serializer(missing_path); // NOLINT(misc-const-correctness)
  EXPECT_FALSE(serializer.good());
  EXPECT_FALSE(serializer.has_next());
  auto b = serializer.pop();
  EXPECT_FALSE(b.has_value());
}
