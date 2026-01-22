/**
 * @file lh_single_pattern_row_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/Csm.h"
#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::decompress::LHChainVerifier;
using crsce::decompress::Csm;
using crsce::common::detail::sha256::sha256_digest;

namespace {
    // Pack a vector<bool> bits MSB-first into 64 bytes
    std::array<std::uint8_t, 64> pack_row_64(const std::vector<bool> &bits) {
        std::array<std::uint8_t, 64> row{};
        std::size_t byte_idx = 0;
        int bit_pos = 0; // 0..7; 0 is MSB position
        std::uint8_t curr = 0;
        for (const bool b: bits) {
            if (b) {
                curr = static_cast<std::uint8_t>(curr | static_cast<std::uint8_t>(1U << (7 - bit_pos)));
            }
            ++bit_pos;
            if (bit_pos >= 8) {
                row.at(byte_idx++) = curr;
                curr = 0;
                bit_pos = 0;
            }
        }
        if (bit_pos != 0) {
            row.at(byte_idx) = curr;
        }
        return row;
    }
} // namespace

TEST(LHChainVerifier, SinglePatternRowOk) { // NOLINT
    // Build a 511-bit alternating pattern in CSM row 0
    Csm csm;
    for (std::size_t c = 0; c < Csm::kS; ++c) {
        csm.put(0, c, (c % 2U) == 0U);
    }

    // Expected row bytes and digest
    std::vector<bool> bits;
    bits.reserve(512);
    for (std::size_t i = 0; i < Csm::kS; ++i) {
        bits.push_back((i % 2U) == 0U);
    }
    bits.push_back(false); // pad bit
    const auto row64 = pack_row_64(bits);

    constexpr std::string seed = "CRSCE_v1_seed";
    const std::vector<std::uint8_t> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());
    std::array<std::uint8_t, 32 + 64> buf{};
    std::ranges::copy(seed_hash, buf.begin());
    std::ranges::copy(row64, std::next(buf.begin(), 32));
    const auto expected = sha256_digest(buf.data(), buf.size());

    std::vector<std::uint8_t> lh_bytes(32U);
    std::ranges::copy(expected, lh_bytes.begin());

    const LHChainVerifier v{seed};
    EXPECT_TRUE(v.verify_rows(csm, lh_bytes, 1));
}
