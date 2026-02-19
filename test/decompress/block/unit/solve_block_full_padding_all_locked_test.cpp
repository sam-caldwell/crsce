/**
 * @file solve_block_full_padding_all_locked_test.cpp
 * @brief Validates that when valid_bits == 0, all cells are pre-locked to 0.
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

TEST(SolveBlock, FullPaddingAllLocked) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    const std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);

    std::vector<std::uint8_t> lh(511U * 32U, 0x5A);
    std::vector<std::uint8_t> sums(4U * 575U, 0x00);
    const std::uint64_t valid_bits = 0ULL;

    Csm csm;
    const auto csums = crsce::decompress::CrossSums::from_packed(std::span<const std::uint8_t>(sums.data(), sums.size()));
    const crsce::decompress::Decompressor dx(std::string(TEST_BINARY_DIR) + "/dx_fullpad_in.crsce",
                                       std::string(TEST_BINARY_DIR) + "/dx_fullpad_out.bin");
    (void)dx.solve_block(lh, csums, csm, valid_bits);

    for (std::uint64_t idx = 0; idx < total_bits; ++idx) {
        const auto r = static_cast<std::size_t>(idx / S);
        const auto c = static_cast<std::size_t>(idx % S);
        EXPECT_TRUE(csm.is_locked(r, c)) << "Expected locked at (" << r << "," << c << ")";
        EXPECT_FALSE(csm.get(r, c)) << "Expected zero at (" << r << "," << c << ")";
    }
}
