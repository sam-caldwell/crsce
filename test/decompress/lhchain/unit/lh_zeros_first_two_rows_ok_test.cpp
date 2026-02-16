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

#include "decompress/RowHashVerifier/RowHashVerifier.h"
#include "decompress/Csm/Csm.h"
#include "common/BitHashBuffer/detail/sha256/sha256_digest.h"

using crsce::decompress::RowHashVerifier;
using crsce::decompress::Csm;
using crsce::common::detail::sha256::sha256_digest;

/**
 * @name RowHashVerifier.ZerosFirstTwoRowsOk
 * @brief Intent: exercise the expected behavior of this test.
 *         Passing indicates the behavior holds; failing indicates a regression.
 *         Assumptions: default environment and explicit setup within this test.
 */
TEST(RowHashVerifier, ZerosFirstTwoRowsOk) { // NOLINT
    const RowHashVerifier v{};
    const Csm csm; // all zeros rows

    // Build expected hashes for the first two zero rows (per-row hashing)
    std::array<std::uint8_t, RowHashVerifier::kRowSize> zero_row{};
    const auto lh0 = sha256_digest(zero_row.data(), zero_row.size());
    const auto lh1 = lh0; // identical zero rows produce identical digests

    std::vector<std::uint8_t> lh_bytes(2U * RowHashVerifier::kHashSize);
    std::ranges::copy(lh0, lh_bytes.begin());
    std::ranges::copy(lh1, std::next(lh_bytes.begin(), 32));

    EXPECT_TRUE(v.verify_rows(csm, lh_bytes, 2));

    // Corrupt one byte; should fail
    lh_bytes[10] ^= 0xFFU;
    EXPECT_FALSE(v.verify_rows(csm, lh_bytes, 2));
}
