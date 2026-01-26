/**
 * @file compress_lh_digest_single_row_integration_test.cpp
 * @brief Integration: Verify LH digest equals sha256(seedHash||rowBytes).
 */
#include "compress/Compress/Compress.h"
#include "common/BitHashBuffer/detail/Sha256Types.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

using crsce::compress::Compress;
using crsce::common::detail::sha256::sha256_digest;
using crsce::common::detail::sha256::u8;

namespace {
    // Pack a vector<bool> bits MSB-first into 64 bytes
    std::array<u8, 64> pack_row_64(const std::vector<bool> &bits) {
        std::array<u8, 64> row{};
        std::size_t byte_idx = 0;
        int bit_pos = 0; // 0..7; 0 is MSB position
        u8 curr = 0;
        for (const bool b: bits) {
            if (b) {
                curr |= static_cast<u8>(1U << (7 - bit_pos));
            }
            ++bit_pos;
            if (bit_pos >= 8) {
                row.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        // Flush any partial byte
        if (bit_pos != 0) {
            row.at(byte_idx) = curr;
        }
        return row;
    }
} // namespace

/**
 * @name CompressIntegration.LHSingleRowMatchesManualSha256
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(CompressIntegration, LHSingleRowMatchesManualSha256) { // NOLINT
    // Build a 511-bit pattern (alternating 1010...) then add one 0 pad bit
    std::vector<bool> bits;
    bits.reserve(512);
    for (std::size_t i = 0; i < Compress::kBitsPerRow; ++i) {
        bits.push_back((i % 2) == 0);
    }
    bits.push_back(false); // pad bit
    const auto row64 = pack_row_64(bits);

    // Compute seed hash (sha256 of seed string)
    const std::string seed = "CRSCE_v1_seed";
    const std::vector<u8> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());

    // Compute expected digest of seed_hash || row64
    std::array<u8, 32 + 64> buf{};
    std::ranges::copy(seed_hash, buf.begin());
    std::ranges::copy(row64, std::next(buf.begin(), 32));
    const auto expected = sha256_digest(buf.data(), buf.size());

    // Drive the Compress class with matching row bits and finalize
    Compress c{"in.bin", "out.bin"};
    for (std::size_t i = 0; i < Compress::kBitsPerRow; ++i) {
        c.push_bit((i % 2) == 0);
    }
    c.finalize_block();
    ASSERT_EQ(c.lh_count(), 1U);
    const auto lh_bytes = c.pop_all_lh_bytes();
    ASSERT_EQ(lh_bytes.size(), 32U);
    for (std::size_t i = 0; i < 32; ++i) { // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        EXPECT_EQ(lh_bytes[i], expected[static_cast<std::size_t>(i)]) // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        << "digest byte mismatch at i=" << i;
    }
    // Sanity on cross-sums: LSM[0] should count ones in the row
    EXPECT_EQ(c.lsm().value(0), static_cast<std::uint16_t>((Compress::kBitsPerRow + 1) / 2));
}
