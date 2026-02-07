/**
 * @file compress_eight_rows_filebitserializer_integration_test.cpp
 * @brief Integration: Feed 8 rows (multiple of 8 bytes) through FileBitSerializer -> Compress
 *        and validate per-row LH and cross-sum aggregates.
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include "helpers/compress_utils.h"
#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <cstdio>
#include <vector>

using crsce::common::FileBitSerializer;
using crsce::compress::Compress;
using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

/**
 * @name CompressIntegration.EightRowsViaFileBitSerializer
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressIntegration, EightRowsViaFileBitSerializer) { // NOLINT
    ASSERT_EQ(Compress::kBitsPerRow % 8, 7U); // 511 % 8 = 7
    const std::size_t kRows = 8; // multiple of 8 rows makes bytes integral
    const std::size_t total_bits = kRows * Compress::kBitsPerRow; // 4088 bits = 511 bytes

    // Build 8 rows with distinct patterns
    std::vector<std::vector<bool> > rows;
    rows.resize(kRows);
    for (std::size_t r = 0; r < kRows; ++r) {
        auto &row = rows[r];
        row.reserve(Compress::kBitsPerRow);
        for (std::size_t c = 0; c < Compress::kBitsPerRow; ++c) {
            bool bit = false;
            switch (r % 4) {
                case 0: bit = ((c % 2) == 0);
                    break; // 1010...
                case 1: bit = ((c % 3) == 0);
                    break; // 100100...
                case 2: bit = ((c % 5) <= 1);
                    break; // 1100110...
                default: bit = ((c ^ r) & 1U) != 0;
                    break; // pseudo-random-ish
            }
            row.push_back(bit);
        }
    }

    // Write exact bytes (no trailing partial garbage) to a temp file
    const std::vector<u8> bytes = crsce::testhelpers::compress::rows_to_bytes(rows);
    ASSERT_EQ(bytes.size(), total_bits / 8);
    const std::string in_path = std::string(TEST_BINARY_DIR) + "/comp8_in.bin";
    const std::string out_path = std::string(TEST_BINARY_DIR) + "/comp8_out.bin";
    {
        std::ofstream f(in_path, std::ios::binary);
        ASSERT_TRUE(f.good());
        const std::vector<char> cbytes(bytes.begin(), bytes.end());
        f.write(cbytes.data(), static_cast<std::streamsize>(cbytes.size()));
    }

    // Stream via FileBitSerializer into Compress
    FileBitSerializer fbs(in_path);
    ASSERT_TRUE(fbs.good());
    Compress cx{in_path, out_path};
    std::size_t fed = 0;
    while (fed < total_bits && fbs.has_next()) {
        const auto b = fbs.pop();
        if (!b.has_value()) { break; }
        cx.push_bit(*b);
        ++fed;
    }
    ASSERT_EQ(fed, total_bits);
    cx.finalize_block();
    ASSERT_EQ(cx.lh_count(), kRows);

    // Compute expected per-row digests across 8 rows
    std::vector<u8> expected_concat;
    expected_concat.reserve(kRows * 32);
    for (std::size_t r = 0; r < kRows; ++r) {
        const auto row64 = crsce::testhelpers::compress::pack_row_with_pad(rows[r]);
        const auto d = sha256_digest(row64.data(), row64.size());
        expected_concat.insert(expected_concat.end(), d.begin(), d.end());
    }

    const auto lh = cx.pop_all_lh_bytes();
    ASSERT_EQ(lh.size(), kRows * 32);
    EXPECT_EQ(lh, expected_concat);

    // Cross-sum sanity: LSM row counts and a few VSM columns
    for (std::size_t r = 0; r < kRows; ++r) {
        std::size_t ones = 0;
        for (const bool b: rows[r]) { if (b) { ++ones; } }
        EXPECT_EQ(cx.lsm().value(r), static_cast<std::uint16_t>(ones));
    }
    for (std::size_t c = 0; c < 6; ++c) {
        std::size_t col_ones = 0;
        for (std::size_t r = 0; r < kRows; ++r) { if (rows[r][c]) { ++col_ones; } }
        EXPECT_EQ(cx.vsm().value(c), static_cast<std::uint16_t>(col_ones));
    }

    // Cleanup temp files
    (void) std::remove(in_path.c_str());
    (void) std::remove(out_path.c_str());
}
