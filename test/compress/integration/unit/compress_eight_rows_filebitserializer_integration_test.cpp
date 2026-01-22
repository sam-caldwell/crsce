/**
 * @file compress_eight_rows_filebitserializer_integration_test.cpp
 * @brief Integration: Feed 8 rows (multiple of 8 bytes) through FileBitSerializer -> Compress
 *        and validate LH chaining and cross-sum aggregates.
 */
#include "common/FileBitSerializer/FileBitSerializer.h"
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <cstdio>
#include <vector>

using crsce::common::FileBitSerializer;
using crsce::compress::Compress;
using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

namespace {
    // Pack 512 bits (511 data + 1 pad) into 64 bytes for hashing checks
    std::array<u8, 64> pack_row_with_pad(const std::vector<bool> &row511) {
        std::array<u8, 64> out{};
        std::size_t byte_idx = 0;
        int bit_pos = 0;
        u8 curr = 0;
        auto push_bit = [&](bool b) {
            if (b) { curr |= static_cast<u8>(1U << (7 - bit_pos)); }
            ++bit_pos;
            if (bit_pos >= 8) {
                out.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        };
        for (const bool b: row511) { push_bit(b); }
        push_bit(false); // pad bit
        if (bit_pos != 0) { out.at(byte_idx) = curr; }
        return out;
    }

    // Convert 8 rows of 511 bits into a byte vector with bit order MSB-first
    std::vector<u8> rows_to_bytes(const std::vector<std::vector<bool> > &rows) {
        std::vector<u8> bytes;
        bytes.reserve((rows.size() * Compress::kBitsPerRow) / 8);
        u8 curr = 0;
        int bit_pos = 0;
        auto push_bit = [&](bool b) {
            if (b) { curr |= static_cast<u8>(1U << (7 - bit_pos)); }
            ++bit_pos;
            if (bit_pos >= 8) {
                bytes.push_back(curr);
                curr = 0;
                bit_pos = 0;
            }
        };
        for (const auto &row: rows) {
            for (const bool b: row) { push_bit(b); }
        }
        if (bit_pos != 0) { bytes.push_back(curr); }
        return bytes;
    }
} // namespace

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
    const std::vector<u8> bytes = rows_to_bytes(rows);
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

    // Compute expected chained digests across 8 rows
    const std::string seed = "CRSCE_v1_seed";
    const std::vector<u8> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());
    std::array<u8, 96> buf{}; // 32 (prev) + 64 (row)
    std::array<u8, 32> prev = seed_hash;
    std::vector<u8> expected_concat;
    expected_concat.reserve(kRows * 32);
    for (std::size_t r = 0; r < kRows; ++r) {
        const auto row64 = pack_row_with_pad(rows[r]);
        std::ranges::copy(prev, buf.begin());
        std::ranges::copy(row64, std::next(buf.begin(), 32));
        const auto d = sha256_digest(buf.data(), buf.size());
        expected_concat.insert(expected_concat.end(), d.begin(), d.end());
        prev = d;
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
