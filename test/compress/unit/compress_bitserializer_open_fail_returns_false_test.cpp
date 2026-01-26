/**
 * @file compress_bitserializer_open_fail_returns_false_test.cpp
 * @brief compress_file should return false if FileBitSerializer cannot open input.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <string>

#include "compress/Compress/Compress.h"

using crsce::compress::Compress;

/**
 * @name CompressCore.BitSerializerOpenFailReturnsFalse
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressCore, BitSerializerOpenFailReturnsFalse) { // NOLINT
    namespace fs = std::filesystem;
    const std::string in = std::string(TEST_BINARY_DIR) + "/c_in_perm.bin";
    const std::string out = std::string(TEST_BINARY_DIR) + "/c_out_perm.crsc";

    // Create a non-empty input file
    {
        std::ofstream f(in, std::ios::binary);
        ASSERT_TRUE(f.good());
        f.put(static_cast<char>(0xAA));
    }
    ASSERT_TRUE(fs::exists(in));
    ASSERT_GT(fs::file_size(in), 0U);

    // Remove read permissions to force FileBitSerializer(in) to fail to open
    std::error_code ec;
    fs::permissions(in, fs::perms::none, fs::perm_options::replace, ec);
    ASSERT_FALSE(ec); // ensure chmod worked

    Compress cx(in, out);
    EXPECT_FALSE(cx.compress_file());

    // Restore permissions to allow cleanup and inspection
    fs::permissions(in, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::add, ec);

    // Output file may have been created and header written before the failure
    // but the function must return false in this scenario.
    if (fs::exists(out)) {
        EXPECT_GE(fs::file_size(out), 28U);
    }
}
