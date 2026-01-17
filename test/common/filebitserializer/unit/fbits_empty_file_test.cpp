/**
 * One-test file: Empty file -> no bits.
 */
#include <gtest/gtest.h>
#include "fbits_helpers.h"
#include "common/FileBitSerializer.h"
#include <fstream>
#include <ios>
#include <string>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, EmptyFileHasNoBits) {
  const std::string empty_path = tmp_dir() + "/empty.tmp";
  { std::ofstream(empty_path, std::ios::binary); }
  FileBitSerializer serializer(empty_path);
  EXPECT_TRUE(serializer.good());
  EXPECT_FALSE(serializer.has_next());
  auto b = serializer.pop();
  EXPECT_FALSE(b.has_value());
  EXPECT_FALSE(serializer.has_next());
  EXPECT_FALSE(serializer.pop().has_value());
  remove_file(empty_path);
}
