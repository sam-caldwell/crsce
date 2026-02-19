/**
 * @file solve_block_prelok_padding_test.cpp
 * @brief Validate that solve_block pre-locks padded cells when valid_bits < 511*511.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>
#include <span>
#include <string>

#include "decompress/Decompressor/Decompressor.h"
#include "decompress/Csm/Csm.h"
#include "decompress/CrossSum/CrossSum.h"

TEST(SolveBlock, PrelocksPaddedCells) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    const std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);

    // Prepare zero-filled payloads of correct sizes
    // LH: 511 digests * 32 bytes each
    std::vector<std::uint8_t> lh(511U * 32U, 0);
    // Sums: 4 * 575 bytes
    std::vector<std::uint8_t> sums(4U * 575U, 0);

    // Choose a small number of valid bits (prefix); remaining should be pre-locked as 0
    const std::uint64_t valid_bits = total_bits > 100 ? (total_bits - 100) : total_bits;
    Csm csm;
    // Invoke solver to trigger pre-locks; return value may be false due to LH mismatch with zeroed LH payloads.
    const auto csums = crsce::decompress::CrossSums::from_packed(std::span<const std::uint8_t>(sums.data(), sums.size()));
    crsce::decompress::Decompressor dx(std::string(TEST_BINARY_DIR) + "/dummy_in.crsce",
                                       std::string(TEST_BINARY_DIR) + "/dummy_out.bin");
    (void)dx.solve_block(lh, csums, csm, valid_bits);

    for (std::uint64_t idx = valid_bits; idx < total_bits; ++idx) {
        const auto r = static_cast<std::size_t>(idx / S);
        const auto c = static_cast<std::size_t>(idx % S);
        EXPECT_TRUE(csm.is_locked(r, c)) << "Expected padded cell locked at (" << r << "," << c << ")";
        EXPECT_FALSE(csm.get(r, c)) << "Expected padded cell value=0 at (" << r << "," << c << ")";
    }
}
