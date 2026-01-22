/**
 * @file fbits_has_next_idempotent_test.cpp
 * @brief One-test file: has_next() idempotence and EOF stability.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <string>

using crsce::common::FileBitSerializer;

/**

 * @name FileBitSerializerTest.HasNextIdempotentAndStableAtEOF

 * @brief Intent: exercise the expected behavior of this test.

 *         Passing indicates the behavior holds; failing indicates a regression.

 *         Assumptions: default environment and explicit setup within this test.

 */

TEST(FileBitSerializerTest, HasNextIdempotentAndStableAtEOF) {
  const std::string path = std::filesystem::temp_directory_path().string()
                           + std::string("/idempotent_has_next.tmp");
  {
    std::ofstream out(path, std::ios::binary);
    constexpr auto v = static_cast<char>(0xA5); // 1010 0101
    out.write(&v, 1);
  }
  FileBitSerializer s(path); // NOLINT(misc-const-correctness)
  // Before reading, multiple has_next() calls must be true and not consume
  EXPECT_TRUE(s.has_next());
  EXPECT_TRUE(s.has_next());
  // Consume 8 bits
  for (int i = 0; i < 8; ++i) {
    auto b = s.pop();
    ASSERT_TRUE(b.has_value());
  }
  // After EOF, has_next must be false and remain false; pop returns nullopt
  EXPECT_FALSE(s.has_next());
  EXPECT_FALSE(s.pop().has_value());
  EXPECT_FALSE(s.has_next());
  std::remove(path.c_str());
}
