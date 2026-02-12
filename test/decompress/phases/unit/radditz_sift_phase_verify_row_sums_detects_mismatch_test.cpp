/**
 * @file radditz_sift_phase_verify_row_sums_detects_mismatch_test.cpp
 * @brief RadditzSift helpers: verify_row_sums detects mismatches.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "decompress/Csm/detail/Csm.h"
#include "decompress/Phases/RadditzSift/verify_row_sums.h"

using crsce::decompress::Csm;
using crsce::decompress::phases::detail::verify_row_sums;

TEST(RadditzSiftPhaseBasic, VerifyRowSumsDetectsMismatch) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    // csm has a single 1 in row 2
    csm.put(2, 10, true);
    std::vector<std::uint16_t> lsm(S, 0);
    lsm[2] = 0; // mismatch on purpose
    EXPECT_FALSE(verify_row_sums(csm, std::span<const std::uint16_t>(lsm.data(), lsm.size())));
}

