/**
 * @file radditz_sift_phase_helpers_work_and_early_exit_when_no_deficits_test.cpp
 * @brief RadditzSift helpers: compute counts/deficits and early-exit when no deficits.
 */
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "decompress/Csm/Csm.h"
#include "decompress/Phases/RadditzSift/compute_col_counts.h"
#include "decompress/Phases/RadditzSift/compute_deficits.h"
#include "decompress/Phases/RadditzSift/deficit_abs_sum.h"
#include "decompress/Phases/RadditzSift/collect_row_candidates.h"
#include "decompress/Phases/RadditzSift/all_deficits_zero.h"

using crsce::decompress::Csm;
using crsce::decompress::phases::detail::compute_col_counts;
using crsce::decompress::phases::detail::compute_deficits;
using crsce::decompress::phases::detail::deficit_abs_sum;
using crsce::decompress::phases::detail::collect_row_candidates;
using crsce::decompress::phases::detail::all_deficits_zero;

TEST(RadditzSiftPhaseBasic, HelpersWorkAndEarlyExitWhenNoDeficits) { // NOLINT
    constexpr std::size_t S = Csm::kS;
    Csm csm; csm.reset();
    // Place some bits in a single column to produce nonzero col_count initially
    csm.set(0, 5);
    csm.set(1, 5);
    auto cols = compute_col_counts(csm);
    ASSERT_EQ(cols[5], 2);
    std::vector<std::uint16_t> vsm(S, 0);
    vsm[5] = 2; // match exactly
    auto def = compute_deficits(cols, std::span<const std::uint16_t>(vsm.data(), vsm.size()));
    EXPECT_TRUE(all_deficits_zero(def));
    EXPECT_EQ(deficit_abs_sum(def), 0U);
    // Candidate collection should be empty for rows that cannot help
    std::vector<std::size_t> from;
    std::vector<std::size_t> to;
    collect_row_candidates(csm, def, 0, from, to);
    EXPECT_TRUE(from.empty());
    EXPECT_TRUE(to.empty());
}
