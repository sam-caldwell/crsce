/**
 * @file fbits_cross_chunk_test.cpp
 * @brief One-test file: Cross-chunk read across >1 KiB boundary.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"
#include "helpers/remove_file.h"
#include "helpers/tmp_dir.h"
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <optional>
#include <string>
#include <vector>

using crsce::common::FileBitSerializer;

/**
 * @name FileBitSerializerTest.CrossChunkReadHandlesBoundaries
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(FileBitSerializerTest, CrossChunkReadHandlesBoundaries) {
  const std::string path = tmp_dir() + "/cross_chunk.tmp";
  {
    std::ofstream out(path, std::ios::binary);
    std::vector<char> buf(FileBitSerializer::kChunkSize + 2,
                          static_cast<char>(0x00));
    buf.front() = static_cast<char>(0xFF);
    buf.back() = static_cast<char>(0x01);
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
  }
  FileBitSerializer serializer(path); // NOLINT(misc-const-correctness)
  for (int i = 0; i < 8; ++i) {
    auto bit = serializer.pop();
    ASSERT_TRUE(bit.has_value());
    EXPECT_TRUE(*bit);
  }
  std::optional<bool> b;
  constexpr std::size_t total_bits = (FileBitSerializer::kChunkSize + 2) * 8;
  for (std::size_t i = 8; i < total_bits - 8; ++i) {
    b = serializer.pop();
    ASSERT_TRUE(b.has_value());
  }
  std::vector<bool> tail;
  for (int i = 0; i < 8; ++i) {
    auto bit = serializer.pop();
    ASSERT_TRUE(bit.has_value());
    tail.push_back(*bit);
  }
  const std::vector<bool> expect = {false, false, false, false,
                                    false, false, false, true};
  EXPECT_EQ(tail, expect);
  remove_file(path);
}
