/**
 * @file lh_zeros_first_two_rows_ok_test.cpp
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>
#include <string>

#include "decompress/LHChainVerifier/LHChainVerifier.h"
#include "decompress/Csm/Csm.h"
#include "common/BitHashBuffer/detail/Sha256.h"

using crsce::decompress::LHChainVerifier;
using crsce::decompress::Csm;
using crsce::common::detail::sha256::sha256_digest;

TEST(LHChainVerifier, ZerosFirstTwoRowsOk) { // NOLINT
    const std::string seed = "CRSCE_v1_seed";
    const LHChainVerifier v{seed};
    const Csm csm; // all zeros rows

    // Build expected LH for first two zero rows
    const std::vector<std::uint8_t> seed_bytes(seed.begin(), seed.end());
    const auto seed_hash = sha256_digest(seed_bytes.data(), seed_bytes.size());
    std::array<std::uint8_t, LHChainVerifier::kRowSize> zero_row{};

    std::array<std::uint8_t, 32 + LHChainVerifier::kRowSize> buf{};
    std::ranges::copy(seed_hash, buf.begin());
    std::ranges::copy(zero_row, std::next(buf.begin(), 32));
    const auto lh0 = sha256_digest(buf.data(), buf.size());

    std::ranges::copy(lh0, buf.begin());
    const auto lh1 = sha256_digest(buf.data(), buf.size());

    std::vector<std::uint8_t> lh_bytes(2U * LHChainVerifier::kHashSize);
    std::ranges::copy(lh0, lh_bytes.begin());
    std::ranges::copy(lh1, std::next(lh_bytes.begin(), 32));

    EXPECT_TRUE(v.verify_rows(csm, lh_bytes, 2));

    // Corrupt one byte; should fail
    lh_bytes[10] ^= 0xFFU;
    EXPECT_FALSE(v.verify_rows(csm, lh_bytes, 2));
}
