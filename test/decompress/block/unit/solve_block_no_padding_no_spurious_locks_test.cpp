/**
 * @file solve_block_no_padding_no_spurious_locks_test.cpp
 * @brief Validates that with no padding (valid_bits == total_bits), there was no padded-tail pre-locking.
 *        As a sanity check, expect fewer than S*S cells are locked after a best-effort solve attempt.
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

TEST(SolveBlock, NoPaddingNoSpuriousLocks) {
    using crsce::decompress::Csm;
    constexpr std::size_t S = Csm::kS;
    const std::uint64_t total_bits = static_cast<std::uint64_t>(S) * static_cast<std::uint64_t>(S);

    // Patterned LH and zero sums
    std::vector<std::uint8_t> lh(511U * 32U, 0xA5);
    std::vector<std::uint8_t> sums(4U * 575U, 0x00);
    const std::uint64_t valid_bits = total_bits;

    Csm csm;
    const auto csums = crsce::decompress::CrossSums::from_packed(std::span<const std::uint8_t>(sums.data(), sums.size()));
    const crsce::decompress::Decompressor dx(std::string(TEST_BINARY_DIR) + "/dx_nopad_in.crsce",
                                       std::string(TEST_BINARY_DIR) + "/dx_nopad_out.bin");
    (void)dx.solve_block(lh, csums, csm, valid_bits);

    // There is no padded region; ensure we didn't lock an entire padded tail.
    // As a basic sanity check, expect fewer than S*S cells are locked.
    std::size_t locked = 0;
    for (std::size_t r = 0; r < S; ++r) {
        for (std::size_t c = 0; c < S; ++c) {
            locked += csm.is_locked(r, c) ? 1U : 0U;
        }
    }
    EXPECT_LT(locked, S * S) << "Expected some cells to remain unlocked when no padding is present";
}
