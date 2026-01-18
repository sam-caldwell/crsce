/**
 * @file fbits_has_next_across_refill_test.cpp
 * @brief One-test file: has_next() across the buffer refill boundary remains correct.
 */
#include <gtest/gtest.h>
#include <fstream>
#include <ios>
#include <filesystem>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdio>
#include "common/FileBitSerializer.h"

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, HasNextAcrossRefillBoundary) {
    const std::string path = std::filesystem::temp_directory_path().string() + "/has_next_refill.tmp";
    {
        std::ofstream out(path, std::ios::binary);
        std::vector<char> buf(FileBitSerializer::kChunkSize + 1, static_cast<char>(0x00));
        buf.front() = static_cast<char>(0xFF); // first byte all ones
        buf.back() = static_cast<char>(0x01); // last byte 0000 0001
        out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    }
    FileBitSerializer s(path); // NOLINT(misc-const-correctness)
    // The first 8 bits are ones
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(s.has_next());
        // multiple checks should not consume
        EXPECT_TRUE(s.has_next());
        auto b = s.pop();
        ASSERT_TRUE(b.has_value());
        EXPECT_TRUE(*b);
    }
    // Consume until the last byte, intermittently checking has_next
    constexpr std::size_t total_bits = (FileBitSerializer::kChunkSize + 1) * 8;
    for (std::size_t i = 8; i < total_bits - 8; ++i) {
        EXPECT_TRUE(s.has_next());
        auto b = s.pop();
        ASSERT_TRUE(b.has_value());
    }
    // Now at the last byte (0x01 -> MSB-first 0000 0001)
    std::vector<bool> last_bits;
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(s.has_next()); // triggers refill when crossing boundary
        auto b = s.pop();
        ASSERT_TRUE(b.has_value());
        last_bits.push_back(*b);
    }
    const std::vector<bool> expect = {false, false, false, false, false, false, false, true};
    EXPECT_EQ(last_bits, expect);
    EXPECT_FALSE(s.has_next());
    std::remove(path.c_str());
}
