/**
 * One-test file: Non-existent file -> not good, no bits.
 */
#include <gtest/gtest.h>
#include "fbits_helpers.h"
#include "common/FileBitSerializer.h"
#include <string>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, MissingFileNotGoodAndEmpty) {
  const std::string missing_path = tmp_dir() + "/definitely_not_here_12345.bin";
  FileBitSerializer serializer(missing_path);
  EXPECT_FALSE(serializer.good());
  EXPECT_FALSE(serializer.has_next());
  auto b = serializer.pop();
  EXPECT_FALSE(b.has_value());
}
