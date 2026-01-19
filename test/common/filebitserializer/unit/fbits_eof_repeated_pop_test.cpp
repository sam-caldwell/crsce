/**
 * @file fbits_eof_repeated_pop_test.cpp
 * @brief One-test file: Repeated pop() after EOF remains std::nullopt and
 * stable.
 */
#include "common/FileBitSerializer.h"
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, RepeatedPopAfterEOFStable) {
  const std::string path =
      std::filesystem::temp_directory_path().string() + "/repeated_pop_eof.tmp";
  {
    std::ofstream out(path, std::ios::binary);
    constexpr auto v = static_cast<char>(0xFF); // 1111 1111
    out.write(&v, 1);
  }
  FileBitSerializer s(path);
  // Consume all 8 bits
  for (int i = 0; i < 8; ++i) {
    auto b = s.pop();
    ASSERT_TRUE(b.has_value());
  }
  // Now at EOF
  ASSERT_FALSE(s.has_next());
  // Repeated pop() remains nullopt without side effects
  for (int i = 0; i < 5; ++i) {
    auto b = s.pop();
    EXPECT_FALSE(b.has_value());
    EXPECT_FALSE(s.has_next());
  }
  std::remove(path.c_str());
}
