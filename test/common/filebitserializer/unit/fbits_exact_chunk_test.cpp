/**
 * @file fbits_exact_chunk_test.cpp
 * @brief One-test file: Exact 1 KiB buffer boundary cases.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <vector>
#include <optional>
#include <fstream>
#include <ios>
#include <string>
#include "helpers/tmp_dir.h"
#include "helpers/remove_file.h"
#include "common/FileBitSerializer.h"

using crsce::common::FileBitSerializer;

TEST(FileBitSerializerTest, ExactChunkSizeBoundaries) {
    const std::string path = tmp_dir() + "/exact_chunk.tmp";
    {
        std::ofstream out(path, std::ios::binary);
        std::vector<char> buf(FileBitSerializer::kChunkSize, static_cast<char>(0x00));
        buf.front() = static_cast<char>(0x80);
        buf.back() = static_cast<char>(0x01);
        out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    }
    FileBitSerializer serializer(path); // NOLINT(misc-const-correctness)
    auto b0 = serializer.pop();
    ASSERT_TRUE(b0.has_value());
    EXPECT_TRUE(*b0);
    for (int i = 0; i < 7; ++i) {
        auto bi = serializer.pop();
        ASSERT_TRUE(bi.has_value());
        EXPECT_FALSE(*bi);
    }
    std::optional<bool> b;
    constexpr std::size_t total_bits = FileBitSerializer::kChunkSize * 8;
    for (std::size_t i = 8; i < total_bits - 8; ++i) {
        b = serializer.pop();
        ASSERT_TRUE(b.has_value());
    }
    std::vector<bool> last_byte;
    for (int i = 0; i < 8; ++i) {
        auto bit = serializer.pop();
        ASSERT_TRUE(bit.has_value());
        last_byte.push_back(*bit);
    }
    const std::vector<bool> expect_last = {false, false, false, false, false, false, false, true};
    EXPECT_EQ(last_byte, expect_last);
    EXPECT_FALSE(serializer.has_next());
    EXPECT_FALSE(serializer.pop().has_value());
    remove_file(path);
}
