/**
 * @file solve_block_prelok_padding_nonzero_lh_test.cpp
 * @brief Validates padded-tail pre-locking when LH is non-zero (patterned) and sums are zero.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>
#include <string>

#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSums.h"

TEST(SolveBlock, PrelocksPaddedCellsWithNonZeroLh) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    const std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);

    // Prepare LH with a non-zero pattern and zero sums
    std::vector<std::uint8_t> lh(511U * 32U, 0xAA);
    std::vector<std::uint8_t> sums(4U * 575U, 0x00);

    // Pre-lock region: a modest padded tail
    const std::uint64_t pad = 257ULL;
    const std::uint64_t valid_bits = (total_bits > pad) ? (total_bits - pad) : 0ULL;

    Csm csm;
    const auto csums = crsce::decompress::CrossSums::from_packed(std::span<const std::uint8_t>(sums.data(), sums.size()));
    const crsce::decompress::Decompressor dx(std::string(TEST_BINARY_DIR) + "/dx_prelock_nonzero_lh_in.crsce",
                                       std::string(TEST_BINARY_DIR) + "/dx_prelock_nonzero_lh_out.bin");
    (void)dx.solve_block(lh, csums, csm, valid_bits);

    for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
        const auto r = static_cast<std::size_t>(idx / S);
        const auto c = static_cast<std::size_t>(idx % S);
        EXPECT_TRUE(csm.is_locked(r, c)) << "Expected padded cell locked at (" << r << "," << c << ")";
        EXPECT_FALSE(csm.get(r, c)) << "Expected padded cell value=0 at (" << r << "," << c << ")";
    }
}
