/**
 * @file fbits_one_byte_test.cpp
 * @brief One-test file: Single byte 0xAC -> MSB-first bits.
 */
#include "common/FileBitSerializer.h"
#include "helpers/read_all.h"
#include "helpers/remove_file.h"
#include "helpers/tmp_dir.h"
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <string>
#include <vector>

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, OneByteBitsAreMSBFirst) {
  const std::string path = tmp_dir() + "/one_byte.tmp";
  {
    std::ofstream out(path, std::ios::binary);
    constexpr auto v = static_cast<char>(0xAC);
    out.write(&v, 1);
  }
  FileBitSerializer serializer(path); // NOLINT(misc-const-correctness)
  auto bits = read_all(serializer);
  const std::vector<bool> expect = {true, false, true,  false,
                                    true, true,  false, false};
  EXPECT_EQ(bits, expect);
  remove_file(path);
}
