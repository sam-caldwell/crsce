/**
 * @file fbits_two_byte_sequence_test.cpp
 * @brief One-test file: Two-byte sequence spans a byte boundary; verify
 * MSB-first order.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <string>
#include <vector>

using crsce::common::FileBitSerializer;

/**
 * @name FileBitSerializerTest.TwoByteSequenceMSBOrder
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(FileBitSerializerTest, TwoByteSequenceMSBOrder) {
  const std::string path =
      std::filesystem::temp_directory_path().string() + "/two_byte_seq.tmp";
  {
    std::ofstream out(path, std::ios::binary);
    constexpr auto b0 = static_cast<char>(0x80); // 1000 0000
    constexpr auto b1 = static_cast<char>(0x01); // 0000 0001
    out.write(&b0, 1);
    out.write(&b1, 1);
  }
  FileBitSerializer s(path); // NOLINT(misc-const-correctness)
  std::vector<bool> bits;
  bits.reserve(16);
  for (int i = 0; i < 16; ++i) {
    ASSERT_TRUE(s.has_next());
    auto b = s.pop();
    ASSERT_TRUE(b.has_value());
    bits.push_back(*b);
  }
  EXPECT_FALSE(s.has_next());
  const std::vector<bool> expect = {true,  false, false, false, false, false,
                                    false, false, false, false, false, false,
                                    false, false, false, true};
  EXPECT_EQ(bits, expect);
  std::remove(path.c_str());
}
